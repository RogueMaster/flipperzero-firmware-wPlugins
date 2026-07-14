# Flipper Zero Development Workflow

**MANDATORY READING FOR ALL SUBAGENTS** - Follow this workflow exactly to ensure code quality and proper testing.

## Development Environment Setup ✅

### Prerequisites
- Python 3.8+ installed
- Virtual environment created: `flipper-dev-env/`
- uFBT installed and working
- Both apps built successfully

### Project Structure
```
flipper-space-calculators/
├── flipper-dev-env/          # Python virtual environment  
├── space-travel-calculator/  # Space Travel Calculator app
│   ├── space_travel_calculator.c
│   ├── application.fam
│   ├── dist/                 # Built .fap files
│   └── .vscode/             # VS Code debugging config
├── dilate/                   # Dilate time calculator app
│   ├── dilate.c
│   ├── application.fam  
│   ├── dist/                 # Built .fap files
│   └── .vscode/             # VS Code debugging config
└── DEV_WORKFLOW.md          # This file
```

## MANDATORY Development Workflow

### Every Code Change Must Follow This Process:

#### 1. Activate Environment
```bash
source flipper-dev-env/bin/activate
```

#### 2. Navigate to App Directory
```bash
cd space-travel-calculator  # or cd dilate
```

#### 3. Build the App
```bash
ufbt
```
**REQUIREMENT**: Build must succeed with no errors before proceeding.

#### 4. Run Quality Checks
```bash
# Lint the code
ufbt lint_all

# Format the code  
ufbt format_all
```
**REQUIREMENT**: All linting errors must be fixed.

#### 5. VS Code Integration (Optional but Recommended)
```bash
# Set up debugging/IntelliSense
ufbt vscode_dist
```

## Testing Requirements

### Build Testing
- **MANDATORY**: Every code change must build successfully
- **Check**: No compilation errors or warnings
- **Verify**: .fap file generated in `dist/` directory

### Code Quality Testing  
- **MANDATORY**: All code must pass `ufbt lint_all`
- **MANDATORY**: All code must be formatted with `ufbt format_all`
- **Check**: No linting warnings or errors

### Deployment Testing (When Flipper Available)
```bash
# Upload and test on actual hardware
ufbt launch
```

## File Organization Rules

### Source Files
- **Main code**: `app_name.c` (e.g., `space_travel_calculator.c`)
- **Manifest**: `application.fam` (app metadata)
- **Icons**: `app_name.png` (10x10 1-bit PNG)
- **Assets**: `images/` directory for additional graphics

### Build Artifacts
- **Debug build**: `dist/debug/app_name_d.elf`
- **Release build**: `dist/app_name.fap` 
- **Never commit**: `dist/` directory contents

## Code Standards

### C Code Requirements
- **Style**: Follow Flipper Zero coding conventions
- **Headers**: Include all necessary Flipper APIs
- **Memory**: No memory leaks (use proper cleanup)
- **Performance**: <50ms response time for UI updates

### Application Manifest
```c
App(
    appid="your_app_id",
    name="Display Name", 
    apptype=FlipperAppType.EXTERNAL,
    entry_point="your_app_main",
    stack_size=2 * 1024,
    fap_category="Tools",  // Use "Tools" not "Examples"
    fap_icon="your_app.png",
)
```

## Error Handling

### Common Build Errors
1. **Missing headers**: Include proper Flipper API headers
2. **Linker errors**: Check function names and signatures
3. **Icon errors**: Ensure PNG is 10x10 1-bit format

### Quality Check Failures
1. **Code formatting**: Run `ufbt format_all` to auto-fix
2. **Linting errors**: Fix manually based on error messages
3. **API compatibility**: Ensure using correct API version

## Performance Requirements

### Response Time Targets
- **UI updates**: <50ms for all button presses
- **Calculations**: Real-time for Space Travel Calculator
- **Display rendering**: Smooth 60fps when animating

### Memory Constraints
- **Stack size**: 2KB default (increase if needed)
- **No dynamic allocation**: Use static arrays
- **Cleanup**: Free all resources on app exit

## Subagent Checklist

**Before submitting any code, verify:**

- [ ] Environment activated (`source flipper-dev-env/bin/activate`)
- [ ] Build succeeds (`ufbt` returns success)
- [ ] Linting passes (`ufbt lint_all` returns clean)
- [ ] Code formatted (`ufbt format_all` applied)
- [ ] .fap file generated in `dist/` directory
- [ ] No compilation warnings
- [ ] App functionality matches requirements
- [ ] Performance targets met (<50ms UI response)

## Emergency Recovery

### Clean Build Issues
```bash
# Clean build artifacts
ufbt clean

# Reset corrupted state
rm -rf ~/.ufbt
source flipper-dev-env/bin/activate
ufbt  # Will re-download SDK
```

### Environment Issues
```bash
# Recreate virtual environment
rm -rf flipper-dev-env
python3 -m venv flipper-dev-env
source flipper-dev-env/bin/activate
pip install --upgrade ufbt
```

---

**CRITICAL**: Never submit code that doesn't build or pass linting. The Flipper Zero has strict requirements and broken code will brick the device during development.