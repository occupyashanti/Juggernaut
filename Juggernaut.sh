#!/bin/bash
# Juggernaut-Core: Advanced Password Cracking Orchestrator
# Version: 1.1 (Enhanced)
# License: Ethical Use Only
# Dependencies: hashcat, john (optional, for some AI/rule concepts), nvidia-smi (optional), jq, python3, screen, awscli (optional)

set -eo pipefail
shopt -s nullglob

# --- Configuration ---
VERSION="1.1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" # Directory of the script itself
CONFIG_DIR="${HOME}/.config/juggernaut"
SESSION_DIR="${CONFIG_DIR}/sessions"
RULES_DIR="${CONFIG_DIR}/ai_rules"
WORDLISTS_DIR="${CONFIG_DIR}/wordlists" # Recommended location for wordlists
HASH_DB="${CONFIG_DIR}/hash_db.json"
CONFIG_FILE="${CONFIG_DIR}/juggernaut.conf"

# --- Default Settings (can be overridden by juggernaut.conf) ---
DEFAULT_HASHCAT_CMD="hashcat"
DEFAULT_PYTHON_CMD="python3"
DEFAULT_AI_RULEGEN_SCRIPT="${SCRIPT_DIR}/ai_rulegen.py" # Assuming ai_rulegen.py is in the same dir or provide full path
DEFAULT_HASH_MODE="1000" # NTLM
DEFAULT_WORDLIST="${WORDLISTS_DIR}/top10k.txt"
DEFAULT_BRUTE_MASK="?a?a?a?a?a?a" # 6 char all charsets
DEFAULT_AI_COMPLEXITY="7"
DEFAULT_AWS_JOB_QUEUE="password-cracking"
DEFAULT_AWS_JOB_DEFINITION="hashcat:latest" # Assumes a pre-configured AWS Batch Job Definition
DEFAULT_AWS_S3_BUCKET_URI="" # e.g., s3://your-juggernaut-bucket/hashes

# --- Load External Configuration ---
if [[ -f "$CONFIG_FILE" ]]; then
    # shellcheck source=/dev/null
    source "$CONFIG_FILE"
    echo -e "\033[32m[+] Loaded configuration from $CONFIG_FILE\033[0m"
else
    echo -e "\033[33m[~] Warning: Configuration file $CONFIG_FILE not found. Using default settings.\033[0m"
    echo -e "\033[33m[~] Consider creating one. A sample is provided in the script comments or documentation.\033[0m"
fi

# --- Assign variables from config or use defaults ---
HASHCAT_CMD="${HASHCAT_CMD:-$DEFAULT_HASHCAT_CMD}"
PYTHON_CMD="${PYTHON_CMD:-$DEFAULT_PYTHON_CMD}"
AI_RULEGEN_SCRIPT="${AI_RULEGEN_SCRIPT:-$DEFAULT_AI_RULEGEN_SCRIPT}"
DEFAULT_HASH_MODE="${USER_DEFAULT_HASH_MODE:-$DEFAULT_HASH_MODE}"
DEFAULT_WORDLIST="${USER_DEFAULT_WORDLIST:-$DEFAULT_WORDLIST}"
DEFAULT_BRUTE_MASK="${USER_DEFAULT_BRUTE_MASK:-$DEFAULT_BRUTE_MASK}"
AI_COMPLEXITY="${USER_AI_COMPLEXITY:-$DEFAULT_AI_COMPLEXITY}"
AWS_JOB_QUEUE="${USER_AWS_JOB_QUEUE:-$DEFAULT_AWS_JOB_QUEUE}"
AWS_JOB_DEFINITION="${USER_AWS_JOB_DEFINITION:-$DEFAULT_AWS_JOB_DEFINITION}"
AWS_S3_BUCKET_URI="${USER_AWS_S3_BUCKET_URI:-$DEFAULT_AWS_S3_BUCKET_URI}"


# --- Global Hardware Info ---
declare -gA HARDWARE

# --- Cleanup Function ---
function cleanup() {
    echo -e "\033[33m[~] Cleaning up temporary files...\033[0m"
    # Add more specific cleanup if needed, e.g., based on session names
    find "$SESSION_DIR" -name "hash_part_*" -type f -delete
    # Consider killing detached screen sessions if script exits prematurely
}
trap cleanup EXIT SIGINT SIGTERM

# --- Dependency Check ---
function check_dependencies() {
    echo -e "\033[34m[*] Checking dependencies...\033[0m"
    local missing_deps=0
    # Core dependencies
    for cmd in "$HASHCAT_CMD" "$PYTHON_CMD" jq screen; do
        if ! command -v "$cmd" &>/dev/null; then
            echo -e "\033[31m[!] Error: Required command '$cmd' not found. Please install it and ensure it's in your PATH.\033[0m" >&2
            missing_deps=1
        fi
    done

    # Optional AWS CLI if cloud mode might be used
    if ! command -v aws &>/dev/null; then
        echo -e "\033[33m[~] Warning: 'aws' CLI not found. Cloud mode will not be available.\033[0m"
    fi

    # Check for ai_rulegen.py if AI Hybrid mode is intended
    if [[ ! -f "$AI_RULEGEN_SCRIPT" ]]; then
        echo -e "\033[33m[~] Warning: AI Rule Generation script '$AI_RULEGEN_SCRIPT' not found. AI Hybrid mode might fail.\033[0m"
    fi


    [[ "$missing_deps" -eq 1 ]] && { echo -e "\033[31m[!] Aborting due to missing core dependencies.\033[0m"; exit 1; }
    echo -e "\033[32m[+] All core dependencies seem to be present.\033[0m"
}

# --- Hardware Detection ---
function detect_hardware() {
    echo -e "\033[34m[*] Detecting hardware...\033[0m"
    HARDWARE[cpu_threads]=$(nproc 2>/dev/null || echo "4") # Default to 4 if nproc fails
    
    # Attempt to count discrete NVIDIA GPUs first
    if command -v nvidia-smi &>/dev/null; then
        HARDWARE[gpu_count]=$(nvidia-smi -L | wc -l)
        if [[ "${HARDWARE[gpu_count]}" -gt 0 ]]; then
            HARDWARE[gpu_mem]=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits | head -1) # In MiB
            echo -e "\033[32m[+] Detected ${HARDWARE[gpu_count]} NVIDIA GPU(s) with ~${HARDWARE[gpu_mem]}MB VRAM (first GPU).\033[0m"
        else
             # Fallback for non-NVIDIA or if nvidia-smi fails for count
            HARDWARE[gpu_count]=$(lspci 2>/dev/null | grep -Ei 'vga|3d controller' | grep -iv 'intel' | wc -l) # Try to exclude integrated Intel
            [[ "${HARDWARE[gpu_count]}" -eq 0 ]] && HARDWARE[gpu_count]=1 # Assume at least 1 if detection is tricky
            echo -e "\033[33m[~] Detected ${HARDWARE[gpu_count]} generic GPU(s) (NVIDIA-smi not conclusive or other vendor).\033[0m"
        fi
    else
        HARDWARE[gpu_count]=$(lspci 2>/dev/null | grep -Ei 'vga|3d controller' | grep -iv 'intel' | wc -l)
        [[ "${HARDWARE[gpu_count]}" -eq 0 ]] && HARDWARE[gpu_count]=1
        echo -e "\033[33m[~] 'nvidia-smi' not found. Detected ${HARDWARE[gpu_count]} generic GPU(s). GPU memory info unavailable.\033[0m"
    fi
    echo -e "\033[32m[+] Detected ${HARDWARE[cpu_threads]} CPU threads.\033[0m"
}

# --- AI Rule Generation ---
function generate_ai_rules() {
    local hash_file="$1"
    local hash_file_checksum
    hash_file_checksum=$(sha1sum "$hash_file" | cut -d' ' -f1)
    local rule_file="${RULES_DIR}/${hash_file_checksum}.rule"
    
    if [[ -f "$rule_file" && -s "$rule_file" ]]; then # Check if file exists and is not empty
        echo -e "\033[32m[AI] Using cached optimized rules: $rule_file\033[0m"
    else
        echo -e "\033[34m[AI] Generating optimized rules for $(basename "$hash_file"). This may take a while...\033[0m"
        # Ensure AI_RULEGEN_SCRIPT is executable or called with python
        if [[ ! -f "$AI_RULEGEN_SCRIPT" ]]; then
            echo -e "\033[31m[!] AI Rulegen script '$AI_RULEGEN_SCRIPT' not found. Cannot generate rules.\033[0m" >&2
            return 1
        fi
        
        # Assuming ai_rulegen.py takes hash_file and complexity as arguments
        # And outputs rules to stdout. Modify if its interface is different.
        # Example: "$PYTHON_CMD" "$AI_RULEGEN_SCRIPT" --hash-file "$hash_file" --complexity "$AI_COMPLEXITY" > "$rule_file"
        # For the original inline python:
        "$PYTHON_CMD" "$AI_RULEGEN_SCRIPT" "$hash_file" "$AI_COMPLEXITY" > "$rule_file"
        # Check if Python script succeeded and created a non-empty rule file
        if [[ $? -ne 0 || ! -s "$rule_file" ]]; then
            echo -e "\033[31m[!] AI rule generation failed or produced an empty file. Check script output/errors.\033[0m" >&2
            rm -f "$rule_file" # Clean up failed/empty rule file
            return 1
        else
            echo -e "\033[32m[AI] Successfully generated rules: $rule_file\033[0m"
        fi
    fi
    echo "$rule_file" # Output the path to the rule file
}

# --- Distributed Cracking ---
function dispatch_workload() {
    local hash_file="$1"
    local attack_mode="$2" # Changed from attack_type to attack_mode for consistency
    local current_hash_mode="${3:-$DEFAULT_HASH_MODE}" # Accept passed hash_mode or use default
    local active_hash_file_basename
    active_hash_file_basename=$(basename "$hash_file" | sed 's/\.[^.]*$//') # Remove extension

    local session_name_base="${active_hash_file_basename}_${attack_mode}"
    local session_name # Will be fully defined in each case

    echo -e "\033[34m[*] Dispatching workload for '$hash_file' using mode '$attack_mode' (Hashcat Mode: $current_hash_mode)\033[0m"

    case $attack_mode in
        brute)
            local num_splits=${HARDWARE[gpu_count]}
            [[ "$num_splits" -lt 1 ]] && num_splits=1 # Ensure at least 1 split
            echo -e "\033[34m[*] Brute-force: Splitting hash file into $num_splits parts (based on GPU count).\033[0m"
            
            # Clean up any previous parts for this specific hash basename to avoid confusion
            rm -f "${SESSION_DIR}/${active_hash_file_basename}_hash_part_"*
            split -n "l/${num_splits}" -d -a 2 "$hash_file" "${SESSION_DIR}/${active_hash_file_basename}_hash_part_"
            
            local i=0
            for part_file in "${SESSION_DIR}/${active_hash_file_basename}_hash_part_"*; do
                if [[ ! -f "$part_file" || ! -s "$part_file" ]]; then
                    echo -e "\033[33m[~] Warning: Split part '$part_file' is empty or not found. Skipping.\033[0m"
                    continue
                fi
                i=$((i+1))
                session_name="${session_name_base}_part${i}_$(date +%Y%m%d%H%M%S)"
                local screen_session_name="jugg_${session_name}"
                echo -e "\033[34m[*] Starting brute-force on '$part_file' in screen session '$screen_session_name' (Hashcat session: '$session_name')\033[0m"
                read -p "    Enter brute-force mask [Default: ${DEFAULT_BRUTE_MASK}]: " current_brute_mask
                current_brute_mask="${current_brute_mask:-$DEFAULT_BRUTE_MASK}"

                # Consider adding --potfile-path and --outfile-path for better management
                screen -dmS "$screen_session_name" \
                    "$HASHCAT_CMD" -m "$current_hash_mode" -a 3 "$part_file" "$current_brute_mask" \
                    --session "$session_name" --status --status-timer=30 -w 3 --self-test-disable \
                    --potfile-path "${SESSION_DIR}/${session_name}.potfile"
                echo -e "\033[32m[+] Screen session '$screen_session_name' started. Attach with: screen -r $screen_session_name\033[0m"
            done
            ;;

        dictionary)
            read -p "    Enter wordlist path [Default: ${DEFAULT_WORDLIST}]: " wordlist_path
            wordlist_path="${wordlist_path:-$DEFAULT_WORDLIST}"
            if [[ ! -f "$wordlist_path" ]]; then
                echo -e "\033[31m[!] Wordlist '$wordlist_path' not found.\033[0m" >&2
                return 1
            fi
            session_name="${session_name_base}_$(date +%Y%m%d%H%M%S)"
            echo -e "\033[34m[*] Starting dictionary attack with '$wordlist_path' (Hashcat session: '$session_name')\033[0m"
            # Add option for rule files if desired
            # Example: read -p "    Enter rule file (optional): " rule_file_path
            "$HASHCAT_CMD" -m "$current_hash_mode" -a 0 "$hash_file" "$wordlist_path" \
                --session "$session_name" --status --status-timer=30 -w 3 \
                --potfile-path "${SESSION_DIR}/${session_name}.potfile"
            ;;

        ai_hybrid)
            local rule_file
            rule_file=$(generate_ai_rules "$hash_file")
            if [[ $? -ne 0 || -z "$rule_file" ]]; then
                echo -e "\033[31m[!] Failed to get AI rules. Aborting AI Hybrid attack.\033[0m" >&2
                return 1
            fi
            read -p "    Enter base wordlist path [Default: ${DEFAULT_WORDLIST}]: " base_wordlist_path
            base_wordlist_path="${base_wordlist_path:-$DEFAULT_WORDLIST}"
            if [[ ! -f "$base_wordlist_path" ]]; then
                echo -e "\033[31m[!] Base wordlist '$base_wordlist_path' not found.\033[0m" >&2
                return 1
            fi
            session_name="${session_name_base}_$(date +%Y%m%d%H%M%S)"
            echo -e "\033[34m[*] Starting AI Hybrid attack with wordlist '$base_wordlist_path' and rules '$rule_file' (Hashcat session: '$session_name')\033[0m"
            "$HASHCAT_CMD" -m "$current_hash_mode" -a 0 "$hash_file" "$base_wordlist_path" -r "$rule_file" \
                --session "$session_name" --status --status-timer=30 -w 3 \
                --potfile-path "${SESSION_DIR}/${session_name}.potfile" # Keeping potfile for hybrid
            ;;

        cloud)
            if ! command -v aws &>/dev/null; then
                 echo -e "\033[31m[!] AWS CLI ('aws') not found. Cloud mode is unavailable.\033[0m" >&2
                 return 1
            fi
            echo -e "\033[34m[*] Preparing for Cloud (AWS Batch) dispatch...\033[0m"
            local s3_hash_file_uri=""
            if [[ -n "$AWS_S3_BUCKET_URI" ]]; then
                local hash_file_s3_name="$(basename "$hash_file")_$(date +%s)"
                echo -e "\033[34m[*] Uploading '$hash_file' to '${AWS_S3_BUCKET_URI}/${hash_file_s3_name}'...\033[0m"
                if aws s3 cp "$hash_file" "${AWS_S3_BUCKET_URI}/${hash_file_s3_name}"; then
                    s3_hash_file_uri="${AWS_S3_BUCKET_URI}/${hash_file_s3_name}"
                    echo -e "\033[32m[+] Upload successful: $s3_hash_file_uri\033[0m"
                else
                    echo -e "\033[31m[!] Failed to upload hash file to S3. Aborting cloud dispatch.\033[0m" >&2
                    return 1
                fi
            else
                echo -e "\033[33m[~] AWS_S3_BUCKET_URI is not configured. You must manually ensure the hash file is accessible by the AWS Batch job.\033[0m"
                read -p "    Enter the S3 URI or accessible path for the hash file in the Batch container: " s3_hash_file_uri
                if [[ -z "$s3_hash_file_uri" ]]; then
                    echo -e "\033[31m[!] No S3 URI provided. Aborting cloud dispatch.\033[0m" >&2
                    return 1
                fi
            fi
            
            local job_name="juggernaut-$(basename "$hash_file" | sed 's/\.[^.]*$//')-$(date +%s)"
            echo -e "\033[34m[*] Submitting AWS Batch job '$job_name'...\033[0m"
            # This command structure assumes your Docker container in AWS Batch is set up to:
            # 1. Accept the S3 URI (or an internal path if you pre-stage it)
            # 2. Download the file if it's an S3 URI
            # 3. Run hashcat with appropriate parameters (these might also need to be passed or configured in the job definition)
            # This is a simplified example; real-world usage would require more robust parameter passing.
            aws batch submit-job \
                --job-name "$job_name" \
                --job-queue "$AWS_JOB_QUEUE" \
                --job-definition "$AWS_JOB_DEFINITION" \
                --container-overrides 'command=["'"$s3_hash_file_uri"'", "-m", "'"$current_hash_mode"'", "--session", "'"$job_name"'"],environment=[{name="HASH_FILE_URI",value="'"$s3_hash_file_uri"'"}]'
            # You might want to parse the output to get the job ID for tracking.
            echo -e "\033[32m[+] AWS Batch job submitted. Monitor its status via AWS console or CLI.\033[0m"
            echo -e "\033[33m[~] Note: Output/potfile retrieval from cloud jobs needs separate handling.\033[0m"
            ;;
        *)
            echo -e "\033[31m[!] Unknown attack mode: $attack_mode\033[0m" >&2
            return 1
            ;;
    esac
}

# --- Session Management ---
function list_sessions() {
    echo -e "\033[34m[*] Available Hashcat sessions (potfiles/restore files) in ${SESSION_DIR}:\033[0m"
    find "$SESSION_DIR" \( -name "*.potfile" -o -name "*.restore" \) -printf "%f\n" | sed -e 's/\.potfile$//' -e 's/\.restore$//' | sort -u
    echo -e "\033[34m[*] Active Juggernaut screen sessions (if any):\033[0m"
    screen -ls | grep 'jugg_'
}

function resume_session() {
    list_sessions
    read -p "Enter session name to resume (e.g., hashfile_brute_part1_datetime): " session_to_resume
    if [[ -z "$session_to_resume" ]]; then
        echo -e "\033[31m[!] No session name provided.\033[0m" >&2
        return 1
    fi

    # Check for restore file first (preferred by hashcat for exact state)
    local restore_file="${SESSION_DIR}/${session_to_resume}.restore"
    local pot_file="${SESSION_DIR}/${session_to_resume}.potfile"

    if [[ -f "$restore_file" ]]; then
        echo -e "\033[32m[+] Resuming session '$session_to_resume' using .restore file...\033[0m"
        "$HASHCAT_CMD" --session "$session_to_resume" --restore
    elif [[ -f "$pot_file" ]]; then
        # If only potfile exists, hashcat usually reuses it but might not be a "true" restore of a running state.
        # For now, let's assume user wants to continue an attack with this session name.
        # A more complex resume would need to know the original command.
        echo -e "\033[33m[~] Warning: Restore file '$restore_file' not found.\033[0m"
        echo -e "\033[33m[~] If you want to view cracked hashes from '$pot_file', use: $HASHCAT_CMD --show --session $session_to_resume --potfile-path $pot_file\033[0m"
        echo -e "\033[33m[~] To attempt to continue a similar attack (command not fully restored), you might need to re-select attack mode with this session name.\033[0m"
        read -p "    Do you want to show cracked hashes from '${session_to_resume}.potfile'? (y/N): " show_cracked
        if [[ "$show_cracked" =~ ^[Yy]$ ]]; then
             "$HASHCAT_CMD" -m "$DEFAULT_HASH_MODE" --show --session "$session_to_resume" --potfile-path "$pot_file" "$DUMMY_HASH_FILE_FOR_SHOW" 
             # Note: Hashcat --show might need a dummy hash file if no hash list is provided, and the hash type.
             # The original hash file and mode would be needed for a perfect --show. This is a simplification.
             echo -e "\033[33m[~] For a more accurate '--show', provide the original hash type and a (dummy) hash file.\033[0m"
        fi
    else
        echo -e "\033[31m[!] Session files for '$session_to_resume' (e.g., .restore or .potfile) not found in ${SESSION_DIR}\033[0m" >&2
        return 1
    fi
}

# --- Main Control ---
function main() {
    echo -e "\033[35m--- Juggernaut-Core v${VERSION} --- \033[0m"
    check_dependencies
    detect_hardware
    
    declare -A MODES_DESC=(
        [1]="Brute-Force Attack"
        [2]="Dictionary Attack"
        [3]="AI Hybrid Attack (Dictionary + AI Rules)"
        [4]="Cloud Attack (AWS Batch)"
        [5]="Resume Session / List Sessions"
    )
    declare -A MODES_KEY=( # Maps selection number to internal key
        [1]="brute"
        [2]="dictionary"
        [3]="ai_hybrid"
        [4]="cloud"
        [5]="resume"
    )

    echo -e "\n\033[36mAvailable Attack Modes:\033[0m"
    for i in "${!MODES_DESC[@]}"; do
        printf "  %s. %s\n" "$i" "${MODES_DESC[$i]}"
    done
    
    local choice
    read -p "Select attack mode (number): " choice
    
    local selected_mode_key="${MODES_KEY[$choice]}"

    if [[ -z "$selected_mode_key" ]]; then
        echo -e "\033[31m[!] Invalid selection. Exiting.\033[0m" >&2
        exit 1
    fi

    echo -e "\033[32m[+] Mode selected: ${MODES_DESC[$choice]}\033[0m"

    if [[ "$selected_mode_key" == "resume" ]]; then
        resume_session
    else
        local hash_file_path
        read -p "Enter path to hash file: " hash_file_path
        if [[ ! -f "$hash_file_path" || ! -r "$hash_file_path" ]]; then
            echo -e "\033[31m[!] Hash file '$hash_file_path' not found or not readable. Exiting.\033[0m" >&2
            exit 1
        fi
        
        local current_attack_hash_mode
        read -p "Enter Hashcat hash mode type [Default: ${DEFAULT_HASH_MODE}]: " current_attack_hash_mode
        current_attack_hash_mode="${current_attack_hash_mode:-$DEFAULT_HASH_MODE}"

        dispatch_workload "$hash_file_path" "$selected_mode_key" "$current_attack_hash_mode"
    fi
    
    echo -e "\033[35m--- Juggernaut-Core task ($selected_mode_key) initiated/completed ---\033[0m"
}

# --- Initialization ---
mkdir -p "$CONFIG_DIR" "$SESSION_DIR" "$RULES_DIR" "$WORDLISTS_DIR"
[[ -f "$HASH_DB" ]] || echo '{}' > "$HASH_DB" # Initialize if not exists; usage of HASH_DB for logging is a TODO.

# --- Sample Config File ---
# To use, save the following as ~/.config/juggernaut/juggernaut.conf
: <<'SAMPLE_CONFIG'
# Sample Juggernaut-Core Configuration File (~/.config/juggernaut/juggernaut.conf)

# Full path to hashcat binary if not in system PATH
# HASHCAT_CMD="/opt/hashcat/hashcat.bin"

# Full path to python3 binary if not in system PATH or want to specify version
# PYTHON_CMD="/usr/bin/python3.9"

# Full path to your custom ai_rulegen.py script if not in Juggernaut's script directory
# AI_RULEGEN_SCRIPT="/opt/juggernaut_custom_rules/ai_rulegen.py"

# Default Hashcat hash type (e.g., 0 for MD5, 1000 for NTLM, 13100 for Kerberos 5 TGS-REP etype 23)
# USER_DEFAULT_HASH_MODE="0"

# Default wordlist for dictionary and AI hybrid attacks
# USER_DEFAULT_WORDLIST="${HOME}/.config/juggernaut/wordlists/rockyou.txt"

# Default mask for brute-force attacks
# USER_DEFAULT_BRUTE_MASK="?l?l?l?l?l?l?l?l" # 8 lowercase chars

# Default complexity for AI rule generation (if your script supports it)
# USER_AI_COMPLEXITY="5"

# AWS Batch Job Queue for cloud attacks
# USER_AWS_JOB_QUEUE="my-custom-hashcat-queue"

# AWS Batch Job Definition for cloud attacks
# USER_AWS_JOB_DEFINITION="my-hashcat-job-definition:3"

# S3 Bucket URI for uploading hash files for cloud attacks (e.g., "s3://my-juggernaut-hashes")
# This script will append a unique filename to this URI.
# USER_AWS_S3_BUCKET_URI="s3://your-juggernaut-bucket/hashes"

SAMPLE_CONFIG

main "$@"