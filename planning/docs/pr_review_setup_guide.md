# Complete PR Review Automation Setup Guide

**Target System:** Mac Mini M4 with 32GB RAM  
**Goal:** Automated PR reviews with static analysis + local AI using Ollama
**Note:** Automated PR reviews by Codex are configured for this repository, but Codex Bot isn't able to run tests because `clang` is unavailable in the environment it runs in. Tests must be run locally, and it's recommended that you do this prior to merging PRs where Codex Bot has found/fixed issues.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Part 1: Install Static Analysis Tools](#part-1-install-static-analysis-tools)
3. [Part 2: Install and Configure Ollama](#part-2-install-and-configure-ollama)
4. [Part 3: Install and Configure PR-Agent](#part-3-install-and-configure-pr-agent)
5. [Part 4: GitHub Actions Workflow](#part-4-github-actions-workflow)
6. [Part 5: Local Testing Script](#part-5-local-testing-script)
7. [Part 6: Usage Guide](#part-6-usage-guide)
8. [Part 7: Troubleshooting](#part-7-troubleshooting)

---

## Prerequisites

- Mac Mini M4 with 32GB RAM ✅
- macOS with Homebrew installed
- GitHub account with a C project repository
- Admin access to your GitHub repository
- Basic familiarity with terminal/command line

**Install Homebrew** (if not already installed):
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

---

## Part 1: Install Static Analysis Tools

### 1.1 Install via Homebrew

```bash
# Update Homebrew
brew update

# Install static analysis tools
brew install cppcheck llvm

# Verify installations
cppcheck --version
clang --version
clang-tidy --version
```

### 1.2 Add LLVM to PATH

Add these lines to your `~/.zshrc` (or `~/.bash_profile` if using bash):

```bash
# LLVM/Clang tools
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
```

Then reload your shell:
```bash
source ~/.zshrc
```

### 1.3 Test Static Analysis Tools

Create a test file with intentional bugs:

```bash
cat > test_bugs.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>

int main() {
    int x = 5;
    int x = 10;  // Double declaration
    
    char *ptr = malloc(100);
    // Memory leak - no free()
    
    int a = 5;
    char *b = a;  // Type mismatch
    
    return 0;
}
EOF
```

Run the tools:
```bash
# Should catch errors
cppcheck --enable=all test_bugs.c

# Should also catch issues
clang-tidy test_bugs.c -- -std=c11
```

Clean up:
```bash
rm test_bugs.c
```

---

## Part 2: Install and Configure Ollama

### 2.1 Install Ollama

```bash
# Download and install Ollama for macOS
curl -fsSL https://ollama.com/install.sh | sh
```

Or download directly from: https://ollama.com/download

### 2.2 Verify Ollama Installation

```bash
# Check if Ollama is running
ollama --version

# Start Ollama service (if not already running)
# Ollama typically starts automatically on macOS
```

### 2.3 Download Recommended Models

For C code review, these models work well on your 32GB M4:

```bash
# Option 1: Qwen 2.5 Coder 32B (Recommended - best quality)
# Requires ~20GB RAM
ollama pull qwen2.5-coder:32b

# Option 2: DeepSeek Coder 33B (Alternative - also excellent)
# Requires ~20GB RAM
ollama pull deepseek-coder:33b

# Option 3: Qwen 2.5 Coder 14B (Lighter alternative)
# Requires ~9GB RAM - use if you want to leave more RAM free
ollama pull qwen2.5-coder:14b

# Option 4: CodeLlama 13B (Backup option)
# Requires ~8GB RAM
ollama pull codellama:13b
```

**Recommendation:** Start with `qwen2.5-coder:32b` - it's specifically trained for code and will give you the best results on your hardware.

### 2.4 Test Ollama

```bash
# Test the model
ollama run qwen2.5-coder:32b "Review this C code and find any bugs: int main() { int *p = malloc(10); return 0; }"
```

You should get a response pointing out the memory leak.

### 2.5 Configure Ollama for Network Access

If you plan to run PR-Agent from a different machine or Docker, enable CORS:

```bash
# Edit Ollama service configuration
# For macOS, Ollama runs as a LaunchAgent

# Set environment variable for CORS
launchctl setenv OLLAMA_ORIGINS "*"

# Restart Ollama
killall ollama
# It will auto-restart
```

To make it permanent, create/edit `~/.ollama/config.json`:
```json
{
  "origins": ["*"]
}
```

---

## Part 3: Install and Configure PR-Agent

### 3.1 Install Python and PR-Agent

```bash
# Ensure Python 3.9+ is installed
python3 --version

# Install PR-Agent
pip3 install pr-agent

# Verify installation
python3 -m pr_agent --help
```

### 3.2 Create Configuration Directory

```bash
# Create config directory in your home folder
mkdir -p ~/.pr-agent
cd ~/.pr-agent
```

### 3.3 Create GitHub Token

1. Go to https://github.com/settings/tokens
2. Click "Generate new token (classic)"
3. Give it a descriptive name: "PR-Agent Token"
4. Select scopes:
   - ✅ `repo` (all sub-options)
   - ✅ `write:discussion`
5. Click "Generate token"
6. **Copy the token immediately** (you won't see it again!)

### 3.4 Create Configuration Files

**Create `~/.pr-agent/.secrets.toml`:**

```bash
cat > ~/.pr-agent/.secrets.toml << 'EOF'
[github]
user_token = "ghp_YOUR_GITHUB_TOKEN_HERE"

# We're using Ollama, so no OpenAI key needed
# But you can add one as fallback if desired
# [openai]
# key = "sk-your-openai-key-here"
EOF
```

**Replace `ghp_YOUR_GITHUB_TOKEN_HERE` with your actual GitHub token!**

**Create `~/.pr-agent/configuration.toml`:**

```bash
cat > ~/.pr-agent/configuration.toml << 'EOF'
[config]
# Use local Ollama model
model = "ollama/qwen2.5-coder:32b"
fallback_models = ["ollama/qwen2.5-coder:32b"]

# Set max tokens based on model's context window
custom_model_max_tokens = 128000

# Important: helps model generate structured output
duplicate_examples = true

# Git provider
git_provider = "github"

[ollama]
# Local Ollama server
api_base = "http://localhost:11434"

[pr_reviewer]
# Focus on issues relevant for C code
require_focused_review = true
require_score_review = false
require_tests_review = true
require_security_review = true

# Inline suggestions are helpful
inline_code_comments = true

# Number of code suggestions to generate
num_code_suggestions = 4

[pr_code_suggestions]
# Auto-improve settings
num_code_suggestions = 4
commitable_code_suggestions = true

[litellm]
# Increase timeout for local models (they can be slower)
request_timeout = 180
EOF
```

### 3.5 Secure Your Configuration Files

```bash
# Make sure tokens are private
chmod 600 ~/.pr-agent/.secrets.toml
chmod 600 ~/.pr-agent/configuration.toml

# Verify permissions
ls -la ~/.pr-agent/
```

### 3.6 Test PR-Agent Locally

Pick one of your existing PRs and test:

```bash
# Test with an actual PR from your repository
python3 -m pr_agent.cli \
  --pr_url https://github.com/YOUR_USERNAME/YOUR_REPO/pull/123 \
  review
```

Replace with your actual GitHub username, repo name, and a real PR number.

You should see:
1. PR-Agent fetching the PR data
2. Ollama processing the request (this may take 30-60 seconds locally)
3. A review posted as a comment on your PR

---

## Part 4: GitHub Actions Workflow

### 4.1 Understand the Workflow Strategy

Since we're using Ollama locally, we have two options:

**Option A: Static Analysis Only in GitHub Actions**
- Fast, free, runs on GitHub's servers
- AI review done manually from your Mac

**Option B: Full Automation (Requires Self-Hosted Runner)**
- Requires setting up your Mac as a GitHub Actions runner
- Both static analysis and AI review run automatically
- More complex setup

**We'll start with Option A** (simpler), and I'll provide Option B instructions separately.

### 4.2 Create the GitHub Actions Workflow (Option A)

In your project repository:

```bash
# Create workflow directory
mkdir -p .github/workflows

# Create the workflow file
cat > .github/workflows/code-review.yml << 'EOF'
name: Automated Code Review

on:
  pull_request:
    types: [opened, synchronize, reopened]

permissions:
  contents: read
  pull-requests: write
  issues: write

jobs:
  static-analysis:
    name: Static Analysis (cppcheck + clang-tidy)
    runs-on: ubuntu-latest
    
    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Install analysis tools
        run: |
          sudo apt-get update
          sudo apt-get install -y cppcheck clang clang-tidy

      - name: Run cppcheck
        id: cppcheck
        run: |
          mkdir -p reports
          
          # Run cppcheck with comprehensive checks
          cppcheck \
            --enable=all \
            --inconclusive \
            --std=c11 \
            --suppress=missingIncludeSystem \
            --suppress=unmatchedSuppression \
            --inline-suppr \
            --template='{file}:{line}:{severity}:{message}' \
            --output-file=reports/cppcheck-report.txt \
            . 2>&1 | tee reports/cppcheck-output.txt
          
          # Store exit code but don't fail the build
          echo "exitcode=$?" >> $GITHUB_OUTPUT
        continue-on-error: true

      - name: Run clang-tidy
        id: clang-tidy
        run: |
          # Find all .c and .h files, excluding build/vendor directories
          find . -type f \( -name "*.c" -o -name "*.h" \) \
            -not -path "*/build/*" \
            -not -path "*/vendor/*" \
            -not -path "*/.git/*" \
            > c_files.txt
          
          # Run clang-tidy if we found files
          if [ -s c_files.txt ]; then
            cat c_files.txt | xargs clang-tidy \
              --checks='-*,clang-analyzer-*,bugprone-*,cert-*,readability-*,performance-*' \
              --warnings-as-errors='' \
              -- -std=c11 \
              2>&1 | tee reports/clang-tidy-output.txt
          else
            echo "No C files found to analyze" | tee reports/clang-tidy-output.txt
          fi
          
          echo "exitcode=$?" >> $GITHUB_OUTPUT
        continue-on-error: true

      - name: Parse and format results
        id: format-results
        run: |
          cat > format_results.py << 'PYTHON_EOF'
import os
import re

def read_file(filepath):
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            return f.read()
    except:
        return ""

def count_issues(text):
    # Count lines that look like issues
    lines = text.split('\n')
    issues = [l for l in lines if l.strip() and not l.startswith('#')]
    return len(issues)

cppcheck_output = read_file('reports/cppcheck-output.txt')
clang_tidy_output = read_file('reports/clang-tidy-output.txt')

cppcheck_count = count_issues(cppcheck_output)
clang_tidy_count = count_issues(clang_tidy_output)

# Truncate if too long
max_length = 4000
cppcheck_display = cppcheck_output[:max_length]
if len(cppcheck_output) > max_length:
    cppcheck_display += f"\n\n... (truncated, {len(cppcheck_output)} total chars)"

clang_tidy_display = clang_tidy_output[:max_length]
if len(clang_tidy_output) > max_length:
    clang_tidy_display += f"\n\n... (truncated, {len(clang_tidy_output)} total chars)"

# Save formatted output
with open('formatted_report.md', 'w') as f:
    f.write(f"## 🔍 Static Analysis Results\n\n")
    
    if cppcheck_count == 0 and clang_tidy_count == 0:
        f.write("✅ **No issues found!** Great job!\n\n")
    else:
        f.write(f"Found **{cppcheck_count + clang_tidy_count}** potential issues.\n\n")
    
    f.write(f"### 📊 cppcheck ({cppcheck_count} issues)\n\n")
    if cppcheck_count == 0:
        f.write("✅ No issues found\n\n")
    else:
        f.write("```\n")
        f.write(cppcheck_display)
        f.write("\n```\n\n")
    
    f.write(f"### 🔧 clang-tidy ({clang_tidy_count} issues)\n\n")
    if clang_tidy_count == 0:
        f.write("✅ No issues found\n\n")
    else:
        f.write("```\n")
        f.write(clang_tidy_display)
        f.write("\n```\n\n")
    
    f.write("---\n\n")
    f.write("💡 **Next step:** Run PR-Agent locally for AI review:\n")
    f.write("```bash\n")
    f.write(f"python3 -m pr_agent.cli --pr_url ${{github.event.pull_request.html_url}} review\n")
    f.write("```\n")
PYTHON_EOF

          python3 format_results.py

      - name: Comment on PR
        uses: actions/github-script@v7
        with:
          github-token: ${{ secrets.GITHUB_TOKEN }}
          script: |
            const fs = require('fs');
            const comment = fs.readFileSync('formatted_report.md', 'utf8');
            
            // Check if we already commented
            const { data: comments } = await github.rest.issues.listComments({
              owner: context.repo.owner,
              repo: context.repo.repo,
              issue_number: context.issue.number,
            });
            
            const botComment = comments.find(comment => 
              comment.user.type === 'Bot' && 
              comment.body.includes('Static Analysis Results')
            );
            
            if (botComment) {
              // Update existing comment
              await github.rest.issues.updateComment({
                owner: context.repo.owner,
                repo: context.repo.repo,
                comment_id: botComment.id,
                body: comment
              });
            } else {
              // Create new comment
              await github.rest.issues.createComment({
                owner: context.repo.owner,
                repo: context.repo.repo,
                issue_number: context.issue.number,
                body: comment
              });
            }

      - name: Upload reports as artifacts
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: static-analysis-reports
          path: reports/
          retention-days: 30
EOF
```

### 4.3 Customize the Workflow for Your Project

Edit `.github/workflows/code-review.yml` and adjust:

1. **C Standard:** Change `--std=c11` to your project's standard (c99, c17, etc.)

2. **File Patterns:** Modify the `find` command if you have different extensions:
   ```yaml
   find . -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" \) \
   ```

3. **Excluded Paths:** Add more paths to exclude:
   ```yaml
   -not -path "*/build/*" \
   -not -path "*/vendor/*" \
   -not -path "*/third_party/*" \
   ```

4. **clang-tidy Checks:** Customize the checks list. See all available checks at:
   https://clang.llvm.org/extra/clang-tidy/checks/list.html

### 4.4 Add .clang-tidy Configuration (Optional)

Create `.clang-tidy` in your project root for project-specific rules:

```yaml
cat > .clang-tidy << 'EOF'
---
Checks: '-*,
  clang-analyzer-*,
  bugprone-*,
  cert-*,
  performance-*,
  readability-*,
  -readability-magic-numbers,
  -readability-isolate-declaration'

WarningsAsErrors: ''

HeaderFilterRegex: '.*'

CheckOptions:
  - key: readability-identifier-naming.VariableCase
    value: lower_case
  - key: readability-identifier-naming.FunctionCase
    value: lower_case
  - key: readability-identifier-naming.TypedefCase
    value: CamelCase
  - key: readability-identifier-naming.StructCase
    value: CamelCase
EOF
```

Adjust naming conventions to match your project's style.

### 4.5 Commit and Push

```bash
# Add the files
git add .github/workflows/code-review.yml
git add .clang-tidy  # if you created it

# Commit
git commit -m "Add automated code review workflow"

# Push
git push
```

---

## Part 5: Local Testing Script

### 5.1 Create Local Review Script

This script lets you test everything locally before pushing:

```bash
# Create the script in your project root
cat > local-review.sh << 'EOF'
#!/bin/bash

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Local Code Review Script ===${NC}\n"

# Check if we're in a git repo
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo -e "${RED}Error: Not in a git repository${NC}"
    exit 1
fi

# Create reports directory
mkdir -p reports

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required tools
echo -e "${YELLOW}Checking for required tools...${NC}"
MISSING_TOOLS=0

if ! command_exists cppcheck; then
    echo -e "${RED}✗ cppcheck not found${NC}"
    MISSING_TOOLS=1
else
    echo -e "${GREEN}✓ cppcheck found${NC}"
fi

if ! command_exists clang-tidy; then
    echo -e "${RED}✗ clang-tidy not found${NC}"
    MISSING_TOOLS=1
else
    echo -e "${GREEN}✓ clang-tidy found${NC}"
fi

if ! command_exists python3; then
    echo -e "${RED}✗ python3 not found${NC}"
    MISSING_TOOLS=1
else
    echo -e "${GREEN}✓ python3 found${NC}"
fi

if [ $MISSING_TOOLS -eq 1 ]; then
    echo -e "\n${RED}Missing required tools. Please install them first.${NC}"
    exit 1
fi

echo ""

# Run cppcheck
echo -e "${YELLOW}Running cppcheck...${NC}"
cppcheck \
    --enable=all \
    --inconclusive \
    --std=c11 \
    --suppress=missingIncludeSystem \
    --inline-suppr \
    --quiet \
    . 2>&1 | tee reports/cppcheck-local.txt

if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo -e "${GREEN}✓ cppcheck completed${NC}\n"
else
    echo -e "${YELLOW}⚠ cppcheck found issues (see reports/cppcheck-local.txt)${NC}\n"
fi

# Run clang-tidy
echo -e "${YELLOW}Running clang-tidy...${NC}"
find . -type f \( -name "*.c" -o -name "*.h" \) \
    -not -path "*/build/*" \
    -not -path "*/vendor/*" \
    -not -path "*/.git/*" \
    > /tmp/c_files_local.txt

if [ -s /tmp/c_files_local.txt ]; then
    cat /tmp/c_files_local.txt | xargs clang-tidy \
        --checks='-*,clang-analyzer-*,bugprone-*,cert-*' \
        --quiet \
        -- -std=c11 \
        2>&1 | tee reports/clang-tidy-local.txt
    
    if [ ${PIPESTATUS[1]} -eq 0 ]; then
        echo -e "${GREEN}✓ clang-tidy completed${NC}\n"
    else
        echo -e "${YELLOW}⚠ clang-tidy found issues (see reports/clang-tidy-local.txt)${NC}\n"
    fi
else
    echo -e "${YELLOW}No C files found to analyze${NC}\n"
fi

# Summary
echo -e "${BLUE}=== Summary ===${NC}"
echo "Reports saved in: ./reports/"
echo "  - cppcheck-local.txt"
echo "  - clang-tidy-local.txt"
echo ""

# Offer to run PR-Agent if on a branch with upstream
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "main" ] && [ "$CURRENT_BRANCH" != "master" ]; then
    echo -e "${YELLOW}Current branch: $CURRENT_BRANCH${NC}"
    echo -e "To get AI review, push your branch and create a PR, then run:"
    echo -e "${GREEN}python3 -m pr_agent.cli --pr_url <PR_URL> review${NC}"
fi

echo -e "\n${GREEN}Done!${NC}"
EOF

# Make it executable
chmod +x local-review.sh
```

### 5.2 Use the Local Review Script

```bash
# Run it in your project directory
./local-review.sh

# View results
cat reports/cppcheck-local.txt
cat reports/clang-tidy-local.txt
```

### 5.3 Add to .gitignore

```bash
# Add reports directory to gitignore
echo "reports/" >> .gitignore
```

---

## Part 6: Usage Guide

### 6.1 Daily Workflow

**When working on code:**

1. **Before committing:**
   ```bash
   ./local-review.sh
   ```
   Fix any critical issues found.

2. **Commit and push:**
   ```bash
   git add .
   git commit -m "Your commit message"
   git push
   ```

3. **Create PR on GitHub:**
   - Go to your repository on GitHub
   - Click "Pull requests" → "New pull request"
   - Create the PR

4. **Wait for static analysis:**
   - GitHub Actions will automatically run
   - Check the "Checks" tab on your PR
   - Review the comment that gets posted

5. **Run AI review locally:**
   ```bash
   # Copy your PR URL and run:
   python3 -m pr_agent.cli --pr_url https://github.com/username/repo/pull/123 review
   ```

6. **Review feedback:**
   - Check the AI review comment on GitHub
   - Address any issues
   - Push updates if needed

### 6.2 PR-Agent Commands

PR-Agent has several useful commands:

```bash
# Full review
python3 -m pr_agent.cli --pr_url <URL> review

# Auto-describe the PR
python3 -m pr_agent.cli --pr_url <URL> describe

# Get improvement suggestions
python3 -m pr_agent.cli --pr_url <URL> improve

# Ask a specific question
python3 -m pr_agent.cli --pr_url <URL> ask "Is there a memory leak in this code?"

# Add documentation
python3 -m pr_agent.cli --pr_url <URL> add_docs

# Update changelog
python3 -m pr_agent.cli --pr_url <URL> update_changelog
```

### 6.3 Create a Helper Script

Make PR reviews even easier:

```bash
cat > ~/bin/review-pr << 'EOF'
#!/bin/bash
# Quick PR review helper

if [ -z "$1" ]; then
    echo "Usage: review-pr <PR-number-or-URL>"
    echo "Example: review-pr 123"
    echo "Example: review-pr https://github.com/user/repo/pull/123"
    exit 1
fi

# If just a number, construct URL
if [[ "$1" =~ ^[0-9]+$ ]]; then
    # Get repo info from git
    REPO_URL=$(git config --get remote.origin.url | sed 's/\.git$//')
    REPO_URL=$(echo $REPO_URL | sed 's/git@github.com:/https:\/\/github.com\//')
    PR_URL="$REPO_URL/pull/$1"
else
    PR_URL="$1"
fi

echo "Reviewing: $PR_URL"
python3 -m pr_agent.cli --pr_url "$PR_URL" review
EOF

chmod +x ~/bin/review-pr

# Make sure ~/bin is in your PATH
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

Now you can simply:
```bash
review-pr 123
```

### 6.4 Batch Review Multiple PRs

If you have multiple PRs to review:

```bash
cat > review-multiple.sh << 'EOF'
#!/bin/bash
# Review multiple PRs at once

PRS=(
    "https://github.com/user/repo/pull/123"
    "https://github.com/user/repo/pull/124"
    "https://github.com/user/repo/pull/125"
)

for pr in "${PRS[@]}"; do
    echo "=== Reviewing $pr ==="
    python3 -m pr_agent.cli --pr_url "$pr" review
    echo ""
    sleep 5  # Brief pause between reviews
done

echo "All reviews completed!"
EOF

chmod +x review-multiple.sh
```

---

## Part 7: Troubleshooting

### 7.1 Ollama Issues

**Problem:** Ollama not responding
```bash
# Check if Ollama is running
pgrep -l ollama

# Restart Ollama
killall ollama
# Wait a few seconds, it should auto-restart

# Or manually start
ollama serve
```

**Problem:** Model not found
```bash
# List installed models
ollama list

# Pull the model again
ollama pull qwen2.5-coder:32b
```

**Problem:** Out of memory
```bash
# Check available RAM
top

# Try a smaller model
ollama pull qwen2.5-coder:14b

# Update configuration.toml to use smaller model
```

### 7.2 PR-Agent Issues

**Problem:** Authentication failed
```bash
# Verify GitHub token
cat ~/.pr-agent/.secrets.toml

# Test token manually
curl -H "Authorization: token YOUR_TOKEN" https://api.github.com/user
```

**Problem:** Cannot find configuration
```bash
# Check config location
ls -la ~/.pr-agent/

# PR-Agent also looks in current directory
# You can copy configs to project root if needed
cp ~/.pr-agent/.secrets.toml .
cp ~/.pr-agent/configuration.toml .
```

**Problem:** Slow responses
```bash
# Increase timeout in configuration.toml
[litellm]
request_timeout = 300  # 5 minutes

# Or use a smaller/faster model
model = "ollama/qwen2.5-coder:14b"
```

**Problem:** "Model not found" error
```bash
# Make sure Ollama is serving the model
ollama list

# Test the model directly
ollama run qwen2.5-coder:32b "test"

# Check the model name in configuration.toml matches exactly
```

### 7.3 GitHub Actions Issues

**Problem:** Workflow not triggering
- Check: `.github/workflows/code-review.yml` is committed and pushed
- Check: Actions are enabled in repository Settings → Actions
- Check: Workflow has correct `on:` triggers

**Problem:** Permission denied errors
```yaml
# Add to workflow file under 'permissions:'
permissions:
  contents: read
  pull-requests: write
  issues: write
```

**Problem:** Comment not posting
- Check: `GITHUB_TOKEN` has write permissions
- Check: Branch protection rules aren't blocking bot comments

### 7.4 Static Analysis Issues

**Problem:** Too many false positives
```bash
# Add suppressions to cppcheck command
cppcheck --suppress=unmatchedSuppression --inline-suppr ...

# Or create cppcheck-suppressions.txt
echo "uninitvar:src/legacy.c" > cppcheck-suppressions.txt
cppcheck --suppressions-list=cppcheck-suppressions.txt ...
```

**Problem:** clang-tidy crashes or hangs
```bash
# Exclude problematic files
find . -name "*.c" -not -path "*/problematic_dir/*" | xargs clang-tidy

# Use fewer checks
clang-tidy --checks='-*,clang-analyzer-*' ...
```

### 7.5 Performance Optimization

**For faster local reviews:**

1. **Use smaller model for quick checks:**
   ```bash
   ollama pull qwen2.5-coder:7b
   # Update configuration.toml accordingly
   ```

2. **Run static analysis on changed files only:**
   ```bash
   # Get list of changed files
   git diff --name-only origin/main | grep '\.c$' | xargs cppcheck
   ```

3. **Parallel processing:**
   ```bash
   # Run tools in parallel
   cppcheck . &
   clang-tidy $(find . -name "*.c") &
   wait
   ```

### 7.6 Getting Help

**Useful resources:**
- PR-Agent docs: https://qodo-merge-docs.qodo.ai/
- Ollama docs: https://github.com/ollama/ollama
- cppcheck manual: https://cppcheck.sourceforge.io/manual.pdf
- clang-tidy checks: https://clang.llvm.org/extra/clang-tidy/

**Debug mode:**
```bash
# Run PR-Agent with verbose logging
python3 -m pr_agent.cli --pr_url <URL> --debug review

# Check Ollama logs
tail -f ~/.ollama/logs/server.log
```

---

## Part 8: Advanced Setup (Optional)

### 8.1 Self-Hosted GitHub Actions Runner

If you want full automation (static analysis + AI review in GitHub Actions):

1. **Install GitHub Actions Runner on your Mac:**
   - Go to your repo → Settings → Actions → Runners → New self-hosted runner
   - Follow the instructions to download and configure

2. **Modify workflow to use self-hosted runner:**
   ```yaml
   jobs:
     combined-review:
       runs-on: self-hosted  # Instead of ubuntu-latest
   ```

3. **Benefits:**
   - Fully automated AI reviews
   - No need to manually run PR-Agent

4. **Drawbacks:**
   - Your Mac must be running and connected
   - Uses your local resources
   - Security consideration (GitHub Actions can run arbitrary code)

### 8.2 Custom Instructions for PR-Agent

Create `~/.pr-agent/custom_instructions.toml`:

```toml
[pr_reviewer.custom_instructions]
# Project-specific instructions for AI reviewer
instructions = """
This is a C project focused on embedded systems. Please pay special attention to:
- Memory management and potential leaks
- Buffer overflows and bounds checking
- Byte order/endianness issues
- Thread safety in concurrent code
- Hardware-specific considerations

Code style follows Linux kernel style with:
- Snake_case for functions and variables
- All-caps for macros and constants
- Tab indentation (8 spaces)
"""
```

### 8.3 Pre-commit Hooks

Run checks automatically before committing:

```bash
# Install pre-commit
pip3 install pre-commit

# Create .pre-commit-config.yaml
cat > .pre-commit-config.yaml << 'EOF'
repos:
  - repo: local
    hooks:
      - id: cppcheck
        name: cppcheck
        entry: cppcheck
        language: system
        args: ['--enable=warning,style', '--inline-suppr', '--quiet']
        files: \.(c|h)$
        
      - id: clang-tidy
        name: clang-tidy
        entry: clang-tidy
        language: system
        args: ['--quiet']
        files: \.(c|h)$
EOF

# Install the hooks
pre-commit install

# Now checks run automatically on git commit!
```

---

## Summary Checklist

- [ ] Static analysis tools installed (cppcheck, clang-tidy)
- [ ] Ollama installed and model downloaded
- [ ] PR-Agent installed and configured
- [ ] GitHub token created and added to .secrets.toml
- [ ] Configuration files created and secured
- [ ] GitHub Actions workflow added to repository
- [ ] Local review script created and tested
- [ ] Reviewed and customized settings for your project
- [ ] Successfully tested on an actual PR

---

## Quick Reference Card

**Local static analysis:**
```bash
./local-review.sh
```

**Review a PR with AI:**
```bash
python3 -m pr_agent.cli --pr_url <URL> review
```

**Test Ollama:**
```bash
ollama run qwen2.5-coder:32b "Review this C code"
```

**Check GitHub Actions status:**
- Go to PR → "Checks" tab

**Update PR-Agent config:**
```bash
nano ~/.pr-agent/configuration.toml
```

---

**You're all set! 🎉**

Start with a simple test PR to verify everything works, then iterate on your configuration as needed.
