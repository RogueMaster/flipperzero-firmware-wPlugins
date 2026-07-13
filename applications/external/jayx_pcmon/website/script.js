// JAYX Website Interactive Logic

document.addEventListener("DOMContentLoaded", () => {
    initFlipperSimulator();
    initTerminalTabs();
    initSetupTabs();
    initAccordion();
    initCopyButtons();
});

/* ==========================================================================
   Flipper Zero Simulator
   ========================================================================== */
function initFlipperSimulator() {
    const displayImg = document.getElementById("display-screen");
    if (!displayImg) return; // Simulator isn't on this page

    const screenInfo = document.getElementById("screen-info");
    const led = document.getElementById("flipper-led");

    // UI Button Elements
    const btnUp = document.getElementById("btn-up");
    const btnDown = document.getElementById("btn-down");
    const btnLeft = document.getElementById("btn-left");
    const btnRight = document.getElementById("btn-right");
    const btnOk = document.getElementById("btn-ok");
    const btnBack = document.getElementById("btn-back");

    // Simulator State
    let currentScreenIdx = 0;
    let specsCardIdx = 0;
    let isFpsIdle = false;

    // Simulation Data Model
    const screens = [
        {
            name: "Main Menu",
            file: "screenshots/MainMenu.png",
            info: "Main Menu • Highlight USB and press OK to start",
            isSpecs: false
        },
        {
            name: "System Stats",
            file: "screenshots/livestats.png",
            info: "System Stats • Real-time CPU, RAM & GPU metrics",
            isSpecs: false
        },
        {
            name: "Game FPS",
            file: "screenshots/fpscounter.png",
            altFile: "screenshots/fpscounterNogame.png",
            info: "Game FPS • Current frame rate (Press OK to toggle idle state)",
            isSpecs: false
        },
        {
            name: "Specs",
            files: [
                "screenshots/systeminfo.png", // Card 0: System
                "screenshots/osinfo.png",     // Card 1: OS
                "screenshots/cpuinfo.png"     // Card 2: CPU
            ],
            info: "Hardware Specs • Press UP / DOWN to cycle cards",
            isSpecs: true
        },
        {
            name: "About Page",
            file: "screenshots/Aboutpage.png",
            info: "About Page • Application and protocol version",
            isSpecs: false
        },
        {
            name: "Bluetooth Page",
            file: "screenshots/Bluetoothpage.png",
            info: "Bluetooth (WIP) • Wireless transport layer page",
            isSpecs: false
        }
    ];

    // Trigger LED Blink on click
    function triggerLedBlink() {
        led.classList.add("active");
        setTimeout(() => {
            led.classList.remove("active");
        }, 150);
    }

    // Update screen display image & text description
    function updateDisplay(withBlink = true) {
        if (withBlink) {
            triggerLedBlink();
        }

        // Add a slight flicker transition to mimic LCD redraw
        displayImg.style.opacity = "0.2";
        
        setTimeout(() => {
            const screen = screens[currentScreenIdx];
            
            if (screen.isSpecs) {
                displayImg.src = screen.files[specsCardIdx];
                screenInfo.innerText = `Screen: Specs (${specsCardIdx + 1}/3) • Up/Down to scroll`;
            } else if (screen.name === "Game FPS") {
                displayImg.src = isFpsIdle ? screen.altFile : screen.file;
                screenInfo.innerText = isFpsIdle ? "Screen: FPS (Idle / No Game)" : "Screen: FPS (Live Tracking)";
            } else {
                displayImg.src = screen.file;
                screenInfo.innerText = `Screen: ${screen.name}`;
            }
            
            displayImg.style.opacity = "0.88";
        }, 60);
    }

    // Button Click Listeners
    btnRight.addEventListener("click", () => {
        currentScreenIdx = (currentScreenIdx + 1) % screens.length;
        specsCardIdx = 0; // reset specs card index
        updateDisplay();
    });

    btnLeft.addEventListener("click", () => {
        currentScreenIdx = (currentScreenIdx - 1 + screens.length) % screens.length;
        specsCardIdx = 0;
        updateDisplay();
    });

    btnUp.addEventListener("click", () => {
        const screen = screens[currentScreenIdx];
        if (screen.isSpecs) {
            specsCardIdx = (specsCardIdx - 1 + screen.files.length) % screen.files.length;
            updateDisplay();
        } else {
            triggerLedBlink();
        }
    });

    btnDown.addEventListener("click", () => {
        const screen = screens[currentScreenIdx];
        if (screen.isSpecs) {
            specsCardIdx = (specsCardIdx + 1) % screen.files.length;
            updateDisplay();
        } else {
            triggerLedBlink();
        }
    });

    btnOk.addEventListener("click", () => {
        const screen = screens[currentScreenIdx];
        if (currentScreenIdx === 0) {
            // Main menu OK -> System Stats
            currentScreenIdx = 1;
            updateDisplay();
        } else if (screen.name === "Game FPS") {
            // FPS -> toggle idle state
            isFpsIdle = !isFpsIdle;
            updateDisplay();
        } else {
            triggerLedBlink();
        }
    });

    btnBack.addEventListener("click", () => {
        if (currentScreenIdx !== 0) {
            currentScreenIdx = 0; // Back to main menu
            specsCardIdx = 0;
            updateDisplay();
        } else {
            triggerLedBlink();
        }
    });

    // Handle Keyboard Arrow Keys when hovering the simulator or container
    const simulatorEl = document.getElementById("simulator");
    simulatorEl.addEventListener("keydown", (e) => {
        if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Enter", "Escape"].includes(e.key)) {
            e.preventDefault(); // prevent scroll
        }
        
        switch (e.key) {
            case "ArrowRight":
                btnRight.click();
                btnRight.focus();
                break;
            case "ArrowLeft":
                btnLeft.click();
                btnLeft.focus();
                break;
            case "ArrowUp":
                btnUp.click();
                btnUp.focus();
                break;
            case "ArrowDown":
                btnDown.click();
                btnDown.focus();
                break;
            case "Enter":
                btnOk.click();
                btnOk.focus();
                break;
            case "Escape":
                btnBack.click();
                btnBack.focus();
                break;
        }
    });

    // Make simulator focusable so keyboard works
    simulatorEl.setAttribute("tabindex", "0");
}

/* ==========================================================================
   Terminal Console Tabs
   ========================================================================== */
function initTerminalTabs() {
    const tabs = document.querySelectorAll("[data-terminal-tab]");
    
    tabs.forEach(tab => {
        tab.addEventListener("click", () => {
            const targetId = tab.getAttribute("data-terminal-tab");
            
            // Remove active classes
            tabs.forEach(t => t.classList.remove("active"));
            document.querySelectorAll(".terminal-content").forEach(content => {
                content.classList.add("hidden");
            });

            // Activate current
            tab.classList.add("active");
            document.getElementById(`terminal-content-${targetId}`).classList.remove("hidden");
        });
    });
}

/* ==========================================================================
   Setup Guide Navigation Tabs
   ========================================================================== */
function initSetupTabs() {
    const tabs = document.querySelectorAll("[data-setup-tab]");
    
    tabs.forEach(tab => {
        tab.addEventListener("click", () => {
            const targetId = tab.getAttribute("data-setup-tab");
            
            // Toggle Tab Nav Buttons
            tabs.forEach(t => t.classList.remove("active"));
            tab.classList.add("active");

            // Toggle Tab Panes
            document.querySelectorAll(".setup-tab-pane").forEach(pane => {
                pane.classList.remove("active");
            });
            document.getElementById(`setup-pane-${targetId}`).classList.add("active");
        });
    });
}

/* ==========================================================================
   Troubleshooting Accordion
   ========================================================================== */
function initAccordion() {
    const items = document.querySelectorAll(".accordion-item");
    
    items.forEach(item => {
        const header = item.querySelector(".accordion-header");
        const content = item.querySelector(".accordion-content");
        
        header.addEventListener("click", () => {
            const isOpen = item.classList.contains("active");
            
            // Close all items
            items.forEach(otherItem => {
                otherItem.classList.remove("active");
                otherItem.querySelector(".accordion-content").style.maxHeight = null;
            });
            
            // Toggle current item
            if (!isOpen) {
                item.classList.add("active");
                content.style.maxHeight = content.scrollHeight + "px";
            }
        });
    });
}

/* ==========================================================================
   Copy to Clipboard Functionality
   ========================================================================== */
function initCopyButtons() {
    const copyButtons = document.querySelectorAll(".btn-copy");
    
    copyButtons.forEach(button => {
        button.addEventListener("click", (e) => {
            e.stopPropagation(); // prevent collapsing accordion if inside one
            
            const targetId = button.getAttribute("data-target");
            let textToCopy = "";
            
            if (targetId) {
                const element = document.getElementById(targetId);
                if (element) {
                    if (element.tagName === "INPUT" || element.tagName === "TEXTAREA") {
                        textToCopy = element.value;
                    } else {
                        textToCopy = element.innerText;
                    }
                }
            } else {
                // If no target is set, try to copy the text inside the sibling code tag
                const pre = button.parentElement.nextElementSibling;
                if (pre && pre.tagName === "PRE") {
                    textToCopy = pre.innerText;
                }
            }
            
            if (!textToCopy) return;

            navigator.clipboard.writeText(textToCopy).then(() => {
                // Visual feedback
                const originalHTML = button.innerHTML;
                
                // If it's the quick command copy button, swap the SVGs
                const iconCopy = button.querySelector(".icon-copy");
                const iconCheck = button.querySelector(".icon-check");
                
                if (iconCopy && iconCheck) {
                    iconCopy.classList.add("hidden");
                    iconCheck.classList.remove("hidden");
                    setTimeout(() => {
                        iconCopy.classList.remove("hidden");
                        iconCheck.classList.add("hidden");
                    }, 2000);
                } else {
                    // Regular text button
                    button.innerText = "Copied!";
                    button.style.color = "#34d058";
                    
                    setTimeout(() => {
                        button.innerHTML = originalHTML;
                        button.style.color = null;
                    }, 2000);
                }
            }).catch(err => {
                console.error("Failed to copy text: ", err);
            });
        });
    });
}
