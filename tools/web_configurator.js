// Web Configurator JavaScript
// ESP32-S3 FIDO2 Macropad Configuration Interface

let port = null;
let reader = null;
let writer = null;
let isConnected = false;
let loadedMacros = new Array(12).fill(null); // Store macros loaded from device
let sequenceSteps = {}; // Store sequence steps for each macro (per OS: {windows: [], mac: []})
let modalSequenceSteps = null; // Working copy for modal editing
let currentSequenceIndex = -1; // Track which macro's sequence is being edited in modal
let currentOS = 'windows'; // Current OS being edited in modal (windows/mac)
let originalMacros = new Array(12).fill(null); // Store original macro state for change detection
let customKeyNames = new Array(12).fill(null); // Store custom key names

// Key names mapping (keycode -> name)
const keyNames = {
    4: 'A', 5: 'B', 6: 'C', 7: 'D', 8: 'E', 9: 'F', 10: 'G', 11: 'H',
    12: 'I', 13: 'J', 14: 'K', 15: 'L', 16: 'M', 17: 'N', 18: 'O', 19: 'P',
    20: 'Q', 21: 'R', 22: 'S', 23: 'T', 24: 'U', 25: 'V', 26: 'W', 27: 'X',
    28: 'Y', 29: 'Z', 30: '1', 31: '2', 32: '3', 33: '4', 34: '5', 35: '6',
    36: '7', 37: '8', 38: '9', 39: '0', 40: 'Enter', 41: 'Esc', 42: 'Backspace',
    43: 'Tab', 44: 'Space', 45: '-', 46: '=', 47: '[', 48: ']', 49: '\\',
    51: ';', 52: "'", 53: '`', 54: ',', 55: '.', 56: '/', 57: 'Caps Lock',
    58: 'F1', 59: 'F2', 60: 'F3', 61: 'F4', 62: 'F5', 63: 'F6', 64: 'F7',
    65: 'F8', 66: 'F9', 67: 'F10', 68: 'F11', 69: 'F12', 70: 'Print Screen',
    71: 'Scroll Lock', 72: 'Pause', 73: 'Insert', 74: 'Home', 75: 'Page Up',
    76: 'Delete', 77: 'End', 78: 'Page Down', 79: 'Right Arrow', 80: 'Left Arrow',
    81: 'Down Arrow', 82: 'Up Arrow', 205: 'Play'
};

// Reverse mapping (name -> keycode) for dropdown
const keyCodes = {};
Object.keys(keyNames).forEach(code => {
    keyCodes[keyNames[code]] = parseInt(code);
});

// Modifier names
const modifierNames = {
    1: 'Ctrl',
    2: 'Shift',
    4: 'Alt',
    8: 'GUI (Cmd/Win)'
};

// Initialize UI
function initUI() {
    generateMacroCards();
    generateLEDConfig();
}

// Default macro configurations with OS-specific sequences
const defaultMacros = [
    {
        label: 'Screenshot',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8, 2], keys: [70], id: 1 } // Win+Shift+PrintScreen
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8, 2], keys: [4], id: 1 } // Cmd+Shift+4
            ]
        }
    },
    {
        label: 'Mic Mute',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [1, 2], keys: [13], id: 1 } // Ctrl+Shift+M
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8, 2], keys: [13], id: 1 } // Cmd+Shift+M
            ]
        }
    },
    {
        label: 'Video Toggle',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [1, 2], keys: [25], id: 1 } // Ctrl+Shift+V
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8, 2], keys: [25], id: 1 } // Cmd+Shift+V
            ]
        }
    },
    {
        label: 'Lock Screen',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [15], id: 1 } // Win+L
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8, 1], keys: [27], id: 1 } // Cmd+Ctrl+Q
            ]
        }
    },
    {
        label: 'Copy',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [1], keys: [6], id: 1 } // Ctrl+C
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [6], id: 1 } // Cmd+C
            ]
        }
    },
    {
        label: 'Cut',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [1], keys: [27], id: 1 } // Ctrl+X
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [27], id: 1 } // Cmd+X
            ]
        }
    },
    {
        label: 'Paste',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [1], keys: [25], id: 1 } // Ctrl+V
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [25], id: 1 } // Cmd+V
            ]
        }
    },
    {
        label: 'Delete',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [], keys: [76], id: 1 } // Delete
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [], keys: [76], id: 1 } // Delete (same on Mac)
            ]
        }
    },
    {
        label: 'Cursor IDE',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [21], id: 1 } // Win+R
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [44], id: 1 } // Cmd+Space
            ]
        }
    },
    {
        label: 'Browser DevTools',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [], keys: [69], id: 1 } // F12
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8, 2], keys: [12], id: 1 } // Cmd+Shift+I
            ]
        }
    },
    {
        label: 'Terminal',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [21], id: 1 }, // Win+R
                { type: 'delay', delay: 200, id: 2 },
                { type: 'text', text: 'cmd', id: 3 },
                { type: 'delay', delay: 100, id: 4 },
                { type: 'key', modifiers: [], keys: [40], id: 5 } // Enter
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [44], id: 1 }, // Cmd+Space
                { type: 'delay', delay: 200, id: 2 },
                { type: 'text', text: 'terminal', id: 3 },
                { type: 'delay', delay: 100, id: 4 },
                { type: 'key', modifiers: [], keys: [40], id: 5 } // Enter
            ]
        }
    },
    {
        label: 'Calculator',
        windows: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [21], id: 1 }, // Win+R
                { type: 'delay', delay: 200, id: 2 },
                { type: 'text', text: 'calc', id: 3 },
                { type: 'delay', delay: 100, id: 4 },
                { type: 'key', modifiers: [], keys: [40], id: 5 } // Enter
            ]
        },
        mac: {
            type: 4,
            sequenceSteps: [
                { type: 'key', modifiers: [8], keys: [44], id: 1 }, // Cmd+Space
                { type: 'delay', delay: 200, id: 2 },
                { type: 'text', text: 'calculator', id: 3 },
                { type: 'delay', delay: 100, id: 4 },
                { type: 'key', modifiers: [], keys: [40], id: 5 } // Enter
            ]
        }
    }
];

function generateMacroCards() {
    const grid = document.getElementById('macrosGrid');
    grid.innerHTML = '';

    for (let i = 0; i < 12; i++) {
        // Get default or loaded macro config
        let macroConfig;
        let keyName;

        if (loadedMacros[i]) {
            // Use loaded config
            macroConfig = loadedMacros[i];
            keyName = customKeyNames[i] ?? macroConfig.keyName ?? '';
        } else {
            // Initial setup: no default names or sequences, user-configurable
            keyName = customKeyNames[i] ?? '';

            // Create OS-specific config
            macroConfig = {
                type: 4, // Always sequence type for defaults
                modifier: 0,
                keycode: 0,
                data: '',
                keyName: keyName
            };

            // Initialize empty sequence steps (no defaults - user configures from scratch)
            if (!sequenceSteps[i]) {
                sequenceSteps[i] = { windows: [], mac: [] };
            }
        }

        // Store original state for change detection
        originalMacros[i] = JSON.parse(JSON.stringify(macroConfig));
        if (!originalMacros[i].keyName) {
            originalMacros[i].keyName = keyName;
        }
        // Store original sequence steps
        originalMacros[i].sequenceSteps = JSON.parse(JSON.stringify(sequenceSteps[i] || { windows: [], mac: [] }));

        const card = document.createElement('div');
        card.className = 'macro-card';
        card.innerHTML = `
            <div class="form-group">
                <label><strong>Key Name</strong></label>
                <input type="text" id="keyName${i}" value="${keyName}" maxlength="12" 
                    placeholder="Enter key name" 
                    oninput="window.updateKeyName(${i}); window.checkMacroChanges(${i})"
                />
                <p class="help-text" style="margin-top: 5px;">Max 12 characters</p>
            </div>
            
            <h3 id="keyTitle${i}" style="margin-top: 0;">Key ${i + 1}${keyName ? ': ' + keyName : ''}</h3>
            
            <div class="form-group">
                <label><strong>What should this key do?</strong></label>
                <select id="macroType${i}" onchange="updateMacroUI(${i}); checkMacroChanges(${i})">
                    <option value="0" ${macroConfig.type === 0 ? 'selected' : ''}>❌ Disabled</option>
                    <option value="4" ${macroConfig.type === 4 ? 'selected' : ''}>🔗 Key Sequence (complex macros)</option>
                </select>
            </div>
            
            
            <!-- Sequence Section -->
            <div id="sequenceSection${i}" class="macro-section" style="display: none;">
                <div class="info-box">
                    <p><strong>🔗 Key Sequences</strong> allow complex multi-step macros:</p>
                </div>
                
                <button type="button" onclick="window.openSequenceModal(${i})" class="btn-open-modal">⚙️ Configure Sequence</button>
            </div>
            
            <!-- Preview -->
            <div class="macro-preview" id="macroPreview${i}">
                <p class="preview-text">No macro configured</p>
            </div>
            
            <div class="macro-actions">
                <button id="saveMacroBtn${i}" onclick="saveMacro(${i + 1})" class="save-btn" disabled>💾 Save Macro</button>
                <button id="cancelMacroBtn${i}" onclick="window.cancelMacroChanges(${i})" class="cancel-btn" disabled>↩️ Reset</button>
            </div>
        `;
        grid.appendChild(card);

        // Initialize UI state for this macro (show correct section, populate info boxes)
        updateMacroUI(i);
    }
}

function generateKeyOptions(selectedKeycode = 0) {
    const keys = Object.keys(keyNames).sort((a, b) => {
        const nameA = keyNames[a];
        const nameB = keyNames[b];
        // Sort letters first, then numbers, then special keys
        if (/^[A-Z]$/.test(nameA) && /^[A-Z]$/.test(nameB)) {
            return nameA.localeCompare(nameB);
        }
        if (/^[A-Z]$/.test(nameA)) return -1;
        if (/^[A-Z]$/.test(nameB)) return 1;
        return nameA.localeCompare(nameB);
    });

    return keys.map(code => {
        const name = keyNames[code];
        const selected = parseInt(code) === selectedKeycode ? 'selected' : '';
        return `<option value="${code}" ${selected}>${name}</option>`;
    }).join('');
}

function updateMacroUI(index) {
    const type = document.getElementById(`macroType${index}`).value;
    const sequenceSection = document.getElementById(`sequenceSection${index}`);

    // Hide all sections
    if (sequenceSection) sequenceSection.style.display = 'none';

    // Show relevant section
    if (type === '4') {
        sequenceSection.style.display = 'block';
        updateSequencePreview(index);
    }

    updateMacroPreview(index);
    checkMacroChanges(index);
}

function getCurrentMacroState(index) {
    const type = parseInt(document.getElementById(`macroType${index}`).value);
    let modifier = 0;
    let keycode = 0;
    let data = '';

    // Get key name
    const nameInput = document.getElementById(`keyName${index}`);
    const keyName = nameInput ? nameInput.value.trim() : '';

    if (type === 3) {
        // Special function
        const specialSelect = document.getElementById(`specialFunction${index}`);
        keycode = specialSelect ? parseInt(specialSelect.value) || 1 : 1;
    } else if (type === 4) {
        // Sequence - compare sequence steps (OS-specific)
        const currentWindowsSteps = sequenceSteps[index]?.windows || [];
        const currentMacSteps = sequenceSteps[index]?.mac || [];
        const originalWindowsSteps = originalMacros[index]?.sequenceSteps?.windows || [];
        const originalMacSteps = originalMacros[index]?.sequenceSteps?.mac || [];
        return {
            type: type,
            keyName: keyName,
            sequenceSteps: JSON.stringify({ windows: currentWindowsSteps, mac: currentMacSteps }),
            originalSequenceSteps: JSON.stringify({ windows: originalWindowsSteps, mac: originalMacSteps })
        };
    }

    return { type, modifier, keycode, data, keyName };
}

function checkMacroChanges(index) {
    if (!originalMacros[index]) return;

    const current = getCurrentMacroState(index);
    const original = originalMacros[index];

    let hasChanges = false;

    if (current.type === 4) {
        hasChanges = current.sequenceSteps !== current.originalSequenceSteps;
    } else if (current.type === 0 && original.type === 0) {
        hasChanges = false;
    } else {
        hasChanges =
            current.type !== original.type ||
            current.modifier !== original.modifier ||
            current.keycode !== original.keycode ||
            current.data !== (original.data || '');
    }

    // Check key name changes
    const currentKeyName = current.keyName || '';
    const originalKeyName = original.keyName || '';
    if (currentKeyName !== originalKeyName) {
        hasChanges = true;
    }

    const saveBtn = document.getElementById(`saveMacroBtn${index}`);
    const cancelBtn = document.getElementById(`cancelMacroBtn${index}`);

    if (saveBtn) {
        saveBtn.disabled = !hasChanges;
    }
    if (cancelBtn) {
        cancelBtn.disabled = !hasChanges;
    }
}

// Make cancel function globally accessible
window.cancelMacroChanges = function (index) {
    if (!originalMacros[index]) return;

    const original = originalMacros[index];
    const typeSelect = document.getElementById(`macroType${index}`);

    if (!typeSelect) return;

    // Reset key name
    const nameInput = document.getElementById(`keyName${index}`);
    const titleElement = document.getElementById(`keyTitle${index}`);
    if (nameInput && original.keyName) {
        nameInput.value = original.keyName;
        if (titleElement) {
            titleElement.textContent = `Key ${index + 1}: ${original.keyName}`;
        }
        customKeyNames[index] = original.keyName;
    }

    // Reset type
    typeSelect.value = original.type;
    updateMacroUI(index);

    // Reset based on type
    if (original.type === 3) {
        // Special function
        const specialSelect = document.getElementById(`specialFunction${index}`);
        if (specialSelect) {
            specialSelect.value = original.keycode || 1;
            updateSpecialFunctionPreview(index);
        }
    } else if (original.type === 4) {
        // Sequence (OS-specific)
        sequenceSteps[index] = original.sequenceSteps ? JSON.parse(JSON.stringify(original.sequenceSteps)) : { windows: [], mac: [] };
        updateSequencePreview(index);
    }

    updateMacroPreview(index);
    checkMacroChanges(index);
};

// Also keep function name for backward compatibility
function cancelMacroChanges(index) {
    window.cancelMacroChanges(index);
}


// Make function globally accessible
window.openSequenceModal = function (index) {
    currentSequenceIndex = index;
    const modal = document.getElementById('sequenceModal');
    const modalTitle = document.getElementById('modalTitle');
    const modalBody = document.getElementById('modalBody');

    if (!modal || !modalTitle || !modalBody) {
        console.error('Modal elements not found!', { modal, modalTitle, modalBody });
        alert('Error: Modal elements not found. Please refresh the page.');
        return;
    }

    // Set title (use custom name if set, otherwise "Key N")
    const keyLabel = customKeyNames[index] || document.getElementById(`keyName${index}`)?.value?.trim() || '';
    modalTitle.textContent = `🔗 Configure Key ${index + 1}${keyLabel ? ': ' + keyLabel : ''}`;

    // Initialize sequence if needed
    if (!sequenceSteps[index]) {
        sequenceSteps[index] = { windows: [], mac: [] };
    }
    if (!sequenceSteps[index].windows) sequenceSteps[index].windows = [];
    if (!sequenceSteps[index].mac) sequenceSteps[index].mac = [];

    // Create a deep copy for modal editing
    modalSequenceSteps = {
        windows: JSON.parse(JSON.stringify(sequenceSteps[index].windows || [])),
        mac: JSON.parse(JSON.stringify(sequenceSteps[index].mac || []))
    };

    // Detect if windows and mac sequences are identical
    const sequencesMatch = areSequencesEqual(modalSequenceSteps.windows, modalSequenceSteps.mac);
    currentOS = sequencesMatch && modalSequenceSteps.windows.length > 0 ? 'both' : 'windows';

    // Build modal content
    modalBody.innerHTML = `
        <div class="info-box">
            <p><strong>🔗 Key Sequences</strong> allow complex multi-step macros:</p>
            <ul>
                <li>Multiple key presses with delays</li>
                <li>Modifier key state management</li>
                <li>Text typing combined with keys</li>
                <li>Example: Win+R → delay 200ms → type "calc" → delay 100ms → Enter</li>
            </ul>
        </div>

        <div class="form-group">
            <label><strong>Operating System</strong></label>
            <div class="os-tabs">
                <button type="button" id="osTabWindows" class="os-tab" onclick="window.setActiveOSTab('windows', ${index})">🪟 Windows</button>
                <button type="button" id="osTabMac" class="os-tab" onclick="window.setActiveOSTab('mac', ${index})">🍎 macOS</button>
                <button type="button" id="osTabBoth" class="os-tab" onclick="window.setActiveOSTab('both', ${index})">🔄 Same for Both</button>
            </div>
        </div>

        <div class="form-group">
            <label><strong>Sequence Steps</strong> <span id="currentOSText">(Windows)</span></label>
            <div id="modalSequenceSteps" class="sequence-steps">
                <!-- Steps will be rendered here -->
            </div>
            <div class="sequence-controls">
                <button type="button" onclick="window.addSequenceStep(${index}, 'key')" class="btn-small">➕ Add Key Press</button>
                <button type="button" onclick="window.addSequenceStep(${index}, 'text')" class="btn-small">📝 Add Text</button>
                <button type="button" onclick="window.addSequenceStep(${index}, 'delay')" class="btn-small">⏱️ Add Delay</button>
                <button type="button" onclick="window.addSequenceStep(${index}, 'release')" class="btn-small">🔓 Release All Keys</button>
            </div>
        </div>
        
        <div class="help-text">
            💡 <strong>Tips:</strong>
            <ul style="margin-top: 5px;">
                <li>Use delays (100-500ms) between actions for reliability</li>
                <li>Release all keys before starting a new sequence</li>
            </ul>
        </div>
        
        <div class="macro-preview">
            <span class="preview-text" id="modalPreview">No steps added yet</span>
        </div>
    `;

    // Initialize OS tabs after modal content is built
    if (currentOS === 'both') {
        document.getElementById('osTabBoth').classList.add('active');
        document.getElementById('currentOSText').textContent = '(Same for Both)';
    } else {
        document.getElementById('osTabWindows').classList.add('active');
        document.getElementById('currentOSText').textContent = '(Windows)';
    }

    // Render steps
    renderSequenceSteps(index, 'modalSequenceSteps');
    updateModalPreview(index);

    // Show modal
    modal.style.display = 'block';
};

// Also keep the function name for backward compatibility
// OS tab management
window.setActiveOSTab = function (os, index) {
    const previousOS = currentOS;
    currentOS = os;

    // Update tab styling
    document.getElementById('osTabWindows').classList.remove('active');
    document.getElementById('osTabMac').classList.remove('active');
    document.getElementById('osTabBoth').classList.remove('active');

    if (os === 'windows') {
        document.getElementById('osTabWindows').classList.add('active');
        document.getElementById('currentOSText').textContent = '(Windows)';
    } else if (os === 'mac') {
        document.getElementById('osTabMac').classList.add('active');
        document.getElementById('currentOSText').textContent = '(macOS)';
    } else if (os === 'both') {
        document.getElementById('osTabBoth').classList.add('active');
        const isInModal = currentSequenceIndex === index;
        const target = isInModal ? modalSequenceSteps : sequenceSteps[index];
        if (!target) return;

        // Copy whichever OS tab was active into the other
        if (previousOS === 'mac') {
            target.windows = JSON.parse(JSON.stringify(target.mac || []));
        } else {
            target.mac = JSON.parse(JSON.stringify(target.windows || []));
        }
        document.getElementById('currentOSText').textContent = '(Same for Both)';
    }

    // Update GUI button label in sequence steps
    updateModalSequenceSteps(index);

    // Update modal preview for current OS
    updateModalPreview(index);
};

// Update modal sequence steps display
function updateModalSequenceSteps(index) {
    renderSequenceSteps(index, 'modalSequenceSteps');
}

function openSequenceModal(index) {
    window.openSequenceModal(index);
}

function closeSequenceModal() {
    const modal = document.getElementById('sequenceModal');
    const indexToUpdate = currentSequenceIndex;
    modal.style.display = 'none';
    currentSequenceIndex = -1;

    // Clear the modal working copy to discard any unsaved changes
    modalSequenceSteps = null;

    // Update preview in the main card
    if (indexToUpdate >= 0) {
        updateSequencePreview(indexToUpdate);
    }
}

function saveSequenceFromModal() {
    if (currentSequenceIndex >= 0) {
        const indexToSave = currentSequenceIndex;

        // Validate sequences before saving
        if (!validateSequences()) {
            return; // Validation failed, don't save
        }

        // In "both" mode, the UI edits only the windows array, so sync it to mac
        if (currentOS === 'both') {
            modalSequenceSteps.mac = JSON.parse(JSON.stringify(modalSequenceSteps.windows || []));
        }

        // Apply changes to main sequenceSteps object
        if (!sequenceSteps[indexToSave]) {
            sequenceSteps[indexToSave] = { windows: [], mac: [] };
        }
        sequenceSteps[indexToSave].windows = JSON.parse(JSON.stringify(modalSequenceSteps.windows || []));
        sequenceSteps[indexToSave].mac = JSON.parse(JSON.stringify(modalSequenceSteps.mac || []));

        updateSequencePreview(indexToSave);
        closeSequenceModal();
        // Check for changes after closing modal
        checkMacroChanges(indexToSave);
    }
}

function validateSequences() {
    if (!modalSequenceSteps) {
        alert('No sequence data available to validate.');
        return false;
    }

    const windowsSteps = modalSequenceSteps.windows || [];
    const macSteps = modalSequenceSteps.mac || [];

    // If "Same for Both" is selected, at least one sequence must be defined
    if (currentOS === 'both') {
        if (windowsSteps.length === 0) {
            alert('Please add at least one step to the sequence before saving.');
            return false;
        }
    } else {
        // If not "Same for Both", both Windows and Mac sequences must be defined
        if (windowsSteps.length === 0 || macSteps.length === 0) {
            alert('Please define sequences for both Windows and macOS, or select "Same for Both" to use the same sequence on both platforms.');
            return false;
        }
    }

    return true;
}

function areSequencesEqual(stepsA, stepsB) {
    if (!stepsA || !stepsB) return (!stepsA && !stepsB);
    if (stepsA.length !== stepsB.length) return false;
    for (let i = 0; i < stepsA.length; i++) {
        const a = stepsA[i], b = stepsB[i];
        if (a.type !== b.type) return false;
        if (a.type === 'key') {
            if (JSON.stringify(a.modifiers || []) !== JSON.stringify(b.modifiers || [])) return false;
            if (JSON.stringify(a.keys || []) !== JSON.stringify(b.keys || [])) return false;
        } else if (a.type === 'text') {
            if (a.text !== b.text) return false;
        } else if (a.type === 'delay') {
            if (a.delay !== b.delay) return false;
        }
    }
    return true;
}

function updateSequencePreview(index) {
    const preview = document.getElementById(`macroPreview${index}`);
    if (!preview) return;
    preview.innerHTML = '';

    if (!sequenceSteps[index]) {
        sequenceSteps[index] = { windows: [], mac: [] };
    }

    const windowsSteps = sequenceSteps[index].windows || [];
    const macSteps = sequenceSteps[index].mac || [];

    if (windowsSteps.length === 0 && macSteps.length === 0) {
        preview.innerHTML = '<p class="preview-text warning">No sequence configured yet. Click "Configure Sequence" to add steps.</p>';
    } else {
        const previews = [];

        if (windowsSteps.length > 0) {
            const winParts = windowsSteps.map(step => getStepPreview(step));
            previews.push(`<p class="preview-text"><strong>Windows:</strong> ${winParts.join(' → ')}</p>`);
        }

        if (macSteps.length > 0) {
            const macParts = macSteps.map(step => getStepPreview(step));
            previews.push(`<p class="preview-text"><strong>macOS:</strong> ${macParts.join(' → ')}</p>`);
        }

        preview.innerHTML = previews.join('');
    }
}

function getStepPreview(step) {
    if (step.type === 'key') {
        const modifiers = [];
        if (step.modifiers) {
            if (step.modifiers.includes(1)) modifiers.push('Ctrl');
            if (step.modifiers.includes(2)) modifiers.push('Shift');
            if (step.modifiers.includes(4)) modifiers.push('Alt');
            if (step.modifiers.includes(8)) modifiers.push('Win/Cmd');
        }

        // Handle multiple keys
        let keyNamesList = [];
        if (step.keys && Array.isArray(step.keys)) {
            keyNamesList = step.keys
                .filter(keycode => keycode && keycode !== 0)
                .map(keycode => keyNames[keycode] || '?');
        } else if (step.keycode && step.keycode !== 0) {
            // Backward compatibility
            keyNamesList = [keyNames[step.keycode] || '?'];
        }

        if (keyNamesList.length === 0) {
            return modifiers.length > 0 ? modifiers.join('+') + '+?' : '?';
        }

        const modStr = modifiers.length > 0 ? modifiers.join('+') + '+' : '';
        return modStr + keyNamesList.join('+');
    } else if (step.type === 'text') {
        return `"${step.text || ''}"`;
    } else if (step.type === 'delay') {
        return `⏱️${step.delay || 0}ms`;
    } else if (step.type === 'release') {
        return '🔓Release';
    }
    return '';
}

function updateModalPreview(index) {
    const previewText = document.getElementById('modalPreview');
    if (!previewText) return;

    if (!modalSequenceSteps) {
        modalSequenceSteps = { windows: [], mac: [] };
    }

    const currentSteps = currentOS === 'both' ? modalSequenceSteps.windows : modalSequenceSteps[currentOS];

    if (!currentSteps || currentSteps.length === 0) {
        previewText.textContent = 'No steps added yet';
        previewText.className = 'preview-text warning';
    } else {
        const parts = currentSteps.map(step => getStepPreview(step));
        previewText.textContent = parts.join(' → ');
        previewText.className = 'preview-text';
    }
}

function initializeSequenceBuilder(index) {
    if (!sequenceSteps[index]) {
        sequenceSteps[index] = { windows: [], mac: [] };
    }
    if (!sequenceSteps[index].windows) sequenceSteps[index].windows = [];
    if (!sequenceSteps[index].mac) sequenceSteps[index].mac = [];
    renderSequenceSteps(index);
}

// Make sequence functions globally accessible
window.addSequenceStep = function (index, stepType) {
    const isInModal = currentSequenceIndex === index;

    // Initialize modalSequenceSteps if needed
    if (isInModal && !modalSequenceSteps) {
        modalSequenceSteps = { windows: [], mac: [] };
    }

    const step = {
        type: stepType,
        id: Date.now() + Math.random()
    };

    if (stepType === 'key') {
        step.modifiers = [];
        step.keys = [0]; // Start with one empty key
        step.keycode = 0; // For backward compatibility
    } else if (stepType === 'text') {
        step.text = '';
    } else if (stepType === 'delay') {
        step.delay = 200;
    } else if (stepType === 'release') {
        // No additional data needed
    }

    // Add to current OS sequence
    if (currentOS === 'both') {
        if (isInModal) {
            modalSequenceSteps.windows.push(JSON.parse(JSON.stringify(step)));
            modalSequenceSteps.mac.push(JSON.parse(JSON.stringify(step)));
        } else {
            if (!sequenceSteps[index]) sequenceSteps[index] = { windows: [], mac: [] };
            if (!sequenceSteps[index].windows) sequenceSteps[index].windows = [];
            if (!sequenceSteps[index].mac) sequenceSteps[index].mac = [];
            sequenceSteps[index].windows.push(JSON.parse(JSON.stringify(step)));
            sequenceSteps[index].mac.push(JSON.parse(JSON.stringify(step)));
        }
    } else {
        if (isInModal) {
            modalSequenceSteps[currentOS].push(step);
        } else {
            if (!sequenceSteps[index]) sequenceSteps[index] = { windows: [], mac: [] };
            if (!sequenceSteps[index][currentOS]) sequenceSteps[index][currentOS] = [];
            sequenceSteps[index][currentOS].push(step);
        }
    }

    const containerId = isInModal ? 'modalSequenceSteps' : null;
    renderSequenceSteps(index, containerId);
    if (isInModal) {
        updateModalPreview(index);
    } else {
        updateMacroPreview(index);
    }
    if (!isInModal) {
        checkMacroChanges(index);
    }
}

window.removeSequenceStep = function (index, stepId) {
    const isInModal = currentSequenceIndex === index;

    if (isInModal && !modalSequenceSteps) return;
    if (!isInModal && !sequenceSteps[index]) return;

    // Remove from current OS sequence
    if (currentOS === 'both') {
        if (isInModal) {
            modalSequenceSteps.windows = modalSequenceSteps.windows.filter(s => s.id !== stepId);
            modalSequenceSteps.mac = modalSequenceSteps.mac.filter(s => s.id !== stepId);
        } else {
            sequenceSteps[index].windows = sequenceSteps[index].windows.filter(s => s.id !== stepId);
            sequenceSteps[index].mac = sequenceSteps[index].mac.filter(s => s.id !== stepId);
        }
    } else {
        if (isInModal) {
            modalSequenceSteps[currentOS] = modalSequenceSteps[currentOS].filter(s => s.id !== stepId);
        } else {
            sequenceSteps[index][currentOS] = sequenceSteps[index][currentOS].filter(s => s.id !== stepId);
        }
    }

    const containerId = isInModal ? 'modalSequenceSteps' : null;
    renderSequenceSteps(index, containerId);
    if (isInModal) {
        updateModalPreview(index);
    } else {
        updateMacroPreview(index);
    }
    if (!isInModal) {
        checkMacroChanges(index);
    }
}

window.moveSequenceStep = function (index, stepId, direction) {
    const isInModal = currentSequenceIndex === index;

    if (isInModal && !modalSequenceSteps) return;
    if (!isInModal && !sequenceSteps[index]) return;

    const steps = currentOS === 'both' ? (isInModal ? modalSequenceSteps.windows : sequenceSteps[index].windows) : (isInModal ? modalSequenceSteps[currentOS] : sequenceSteps[index][currentOS]);
    const idx = steps.findIndex(s => s.id === stepId);
    if (idx === -1) return;

    if (direction === 'up' && idx > 0) {
        [steps[idx], steps[idx - 1]] = [steps[idx - 1], steps[idx]];
    } else if (direction === 'down' && idx < steps.length - 1) {
        [steps[idx], steps[idx + 1]] = [steps[idx + 1], steps[idx]];
    }

    // If 'both' mode, also update the other OS
    if (currentOS === 'both') {
        const otherSteps = isInModal ? (currentOS === 'windows' ? modalSequenceSteps.mac : modalSequenceSteps.windows) : (currentOS === 'windows' ? sequenceSteps[index].mac : sequenceSteps[index].windows);
        if (direction === 'up' && idx > 0) {
            [otherSteps[idx], otherSteps[idx - 1]] = [otherSteps[idx - 1], otherSteps[idx]];
        } else if (direction === 'down' && idx < otherSteps.length - 1) {
            [otherSteps[idx], otherSteps[idx + 1]] = [otherSteps[idx + 1], otherSteps[idx]];
        }
    }

    const containerId = isInModal ? 'modalSequenceSteps' : null;
    renderSequenceSteps(index, containerId);
    if (isInModal) {
        updateModalPreview(index);
    } else {
        updateMacroPreview(index);
    }
    if (!isInModal) {
        checkMacroChanges(index);
    }
}

function renderSequenceSteps(index, containerId = null) {
    const containerIdToUse = containerId || `sequenceSteps${index}`;
    const container = document.getElementById(containerIdToUse);
    if (!container) return;

    const isInModal = containerId === 'modalSequenceSteps';

    if (isInModal) {
        if (!modalSequenceSteps) {
            modalSequenceSteps = { windows: [], mac: [] };
        }
        if (!modalSequenceSteps.windows) modalSequenceSteps.windows = [];
        if (!modalSequenceSteps.mac) modalSequenceSteps.mac = [];
    } else {
        if (!sequenceSteps[index]) {
            sequenceSteps[index] = { windows: [], mac: [] };
        }
        if (!sequenceSteps[index].windows) sequenceSteps[index].windows = [];
        if (!sequenceSteps[index].mac) sequenceSteps[index].mac = [];
    }

    const currentSteps = currentOS === 'both' ? (isInModal ? modalSequenceSteps.windows : sequenceSteps[index].windows) : (isInModal ? modalSequenceSteps[currentOS] : sequenceSteps[index][currentOS]);

    if (!currentSteps || currentSteps.length === 0) {
        container.innerHTML = '<p class="help-text">No steps added yet. Click buttons above to add steps.</p>';
        return;
    }

    container.innerHTML = currentSteps.map((step, idx) => {
        let stepHtml = `<div class="sequence-step" data-step-id="${step.id}">`;
        stepHtml += `<div class="step-number">${idx + 1}</div>`;
        stepHtml += `<div class="step-content">`;

        if (step.type === 'key') {
            // Ensure step.keys is an array for backward compatibility
            if (!step.keys) {
                step.keys = step.keycode ? [step.keycode] : [];
            }
            if (!Array.isArray(step.keys)) {
                step.keys = [step.keys];
            }

            stepHtml += `
                <label>Key Press:</label>
                <div class="step-modifiers">
                    <label class="checkbox-label-small">
                        <input type="checkbox" ${step.modifiers && step.modifiers.includes(1) ? 'checked' : ''}
                               onchange="window.updateSequenceStepModifier(${index}, ${step.id}, 1, this.checked)">
                        <span>Ctrl</span>
                    </label>
                    <label class="checkbox-label-small">
                        <input type="checkbox" ${step.modifiers && step.modifiers.includes(2) ? 'checked' : ''}
                               onchange="window.updateSequenceStepModifier(${index}, ${step.id}, 2, this.checked)">
                        <span>Shift</span>
                    </label>
                    <label class="checkbox-label-small">
                        <input type="checkbox" ${step.modifiers && step.modifiers.includes(4) ? 'checked' : ''}
                               onchange="window.updateSequenceStepModifier(${index}, ${step.id}, 4, this.checked)">
                        <span>Alt</span>
                    </label>
                    <label class="checkbox-label-small">
                        <input type="checkbox" ${step.modifiers && step.modifiers.includes(8) ? 'checked' : ''}
                               onchange="window.updateSequenceStepModifier(${index}, ${step.id}, 8, this.checked)">
                        <span>Win/Cmd</span>
                    </label>
                </div>
                <div class="step-keys" id="step-keys-${step.id}">
                    ${step.keys.map((keycode, keyIndex) => `
                        <div class="key-selection">
                            <select onchange="window.updateSequenceStepKey(${index}, ${step.id}, ${keyIndex}, this.value)">
                                <option value="0">-- Select Key --</option>
                                ${generateKeyOptions(keycode || 0)}
                            </select>
                            <button type="button" onclick="window.removeStepKey(${index}, ${step.id}, ${keyIndex})" class="btn-small" ${step.keys.length <= 1 ? 'disabled' : ''}>❌</button>
                        </div>
                    `).join('')}
                    <button type="button" onclick="window.addStepKey(${index}, ${step.id})" class="btn-small">➕ Add Key</button>
                </div>
            `;
        } else if (step.type === 'text') {
            stepHtml += `
                <label>Type Text:</label>
                <input type="text" value="${step.text || ''}" maxlength="20" 
                       oninput="window.updateSequenceStepText(${index}, ${step.id}, this.value)"
                       placeholder="e.g., calc">
            `;
        } else if (step.type === 'delay') {
            stepHtml += `
                <label>Delay (ms):</label>
                <input type="number" value="${step.delay || 200}" min="0" max="5000" step="50"
                       oninput="window.updateSequenceStepDelay(${index}, ${step.id}, this.value)">
            `;
        } else if (step.type === 'release') {
            stepHtml += `<strong>🔓 Release All Keys</strong>`;
        }

        stepHtml += `</div>`;
        stepHtml += `<div class="step-actions">`;
        stepHtml += `<button type="button" onclick="window.moveSequenceStep(${index}, ${step.id}, 'up')" ${idx === 0 ? 'disabled' : ''} class="btn-icon">⬆️</button>`;
        stepHtml += `<button type="button" onclick="window.moveSequenceStep(${index}, ${step.id}, 'down')" ${idx === currentSteps.length - 1 ? 'disabled' : ''} class="btn-icon">⬇️</button>`;
        stepHtml += `<button type="button" onclick="window.removeSequenceStep(${index}, ${step.id})" class="btn-icon">❌</button>`;
        stepHtml += `</div>`;
        stepHtml += `</div>`;

        return stepHtml;
    }).join('');
}

window.updateSequenceStepModifier = function (index, stepId, modifierValue, checked) {
    const isInModal = currentSequenceIndex === index;
    const currentSteps = currentOS === 'both' ? (isInModal ? modalSequenceSteps.windows : sequenceSteps[index].windows) : (isInModal ? modalSequenceSteps[currentOS] : sequenceSteps[index][currentOS]);
    const step = currentSteps.find(s => s.id === stepId);
    if (!step) return;
    if (!step.modifiers) step.modifiers = [];
    if (checked) {
        if (!step.modifiers.includes(modifierValue)) step.modifiers.push(modifierValue);
    } else {
        step.modifiers = step.modifiers.filter(m => m !== modifierValue);
    }
    if (isInModal) {
        updateModalPreview(index);
    } else {
        updateMacroPreview(index);
    }
    if (!isInModal) {
        checkMacroChanges(index);
    }
}

window.updateSequenceStepKey = function (index, stepId, keyIndex, keycode) {
    const isInModal = currentSequenceIndex === index;
    const currentSteps = currentOS === 'both' ? (isInModal ? modalSequenceSteps.windows : sequenceSteps[index].windows) : (isInModal ? modalSequenceSteps[currentOS] : sequenceSteps[index][currentOS]);
    const step = currentSteps.find(s => s.id === stepId);
    if (step) {
        // Ensure step.keys is an array
        if (!step.keys) step.keys = [];
        if (!Array.isArray(step.keys)) step.keys = [step.keys];

        // Update the specific key at keyIndex
        step.keys[keyIndex] = parseInt(keycode);

        // For backward compatibility, set keycode to the first key if it exists
        step.keycode = step.keys.length > 0 ? step.keys[0] : 0;

        if (isInModal) {
            updateModalPreview(index);
        } else {
            updateMacroPreview(index);
        }
        if (!isInModal) {
            checkMacroChanges(index);
        }
    }
}

window.updateSequenceStepText = function (index, stepId, text) {
    const isInModal = currentSequenceIndex === index;
    const currentSteps = currentOS === 'both' ? (isInModal ? modalSequenceSteps.windows : sequenceSteps[index].windows) : (isInModal ? modalSequenceSteps[currentOS] : sequenceSteps[index][currentOS]);
    const step = currentSteps.find(s => s.id === stepId);
    if (step) {
        step.text = text;
        if (isInModal) {
            updateModalPreview(index);
        } else {
            updateMacroPreview(index);
        }
        if (!isInModal) {
            checkMacroChanges(index);
        }
    }
}

window.updateSequenceStepDelay = function (index, stepId, delay) {
    const isInModal = currentSequenceIndex === index;
    const currentSteps = currentOS === 'both' ? (isInModal ? modalSequenceSteps.windows : sequenceSteps[index].windows) : (isInModal ? modalSequenceSteps[currentOS] : sequenceSteps[index][currentOS]);
    const step = currentSteps.find(s => s.id === stepId);
    if (step) {
        step.delay = parseInt(delay);
        if (isInModal) {
            updateModalPreview(index);
        } else {
            updateMacroPreview(index);
        }
        if (!isInModal) {
            checkMacroChanges(index);
        }
    }
}

// Multi-key support functions
window.addStepKey = function (index, stepId) {
    const isInModal = currentSequenceIndex === index;
    const currentSteps = currentOS === 'both' ? (isInModal ? modalSequenceSteps.windows : sequenceSteps[index].windows) : (isInModal ? modalSequenceSteps[currentOS] : sequenceSteps[index][currentOS]);
    const step = currentSteps.find(s => s.id === stepId);
    if (step) {
        if (!step.keys) step.keys = [];
        step.keys.push(0); // Add empty key
        const containerId = isInModal ? 'modalSequenceSteps' : null;
        renderSequenceSteps(index, containerId);
        if (isInModal) {
            updateModalPreview(index);
        } else {
            updateMacroPreview(index);
        }
        if (!isInModal) {
            checkMacroChanges(index);
        }
    }
}

window.removeStepKey = function (index, stepId, keyIndex) {
    const isInModal = currentSequenceIndex === index;
    const currentSteps = currentOS === 'both' ? (isInModal ? modalSequenceSteps.windows : sequenceSteps[index].windows) : (isInModal ? modalSequenceSteps[currentOS] : sequenceSteps[index][currentOS]);
    const step = currentSteps.find(s => s.id === stepId);
    if (step && step.keys && step.keys.length > 1) {
        step.keys.splice(keyIndex, 1);
        const containerId = isInModal ? 'modalSequenceSteps' : null;
        renderSequenceSteps(index, containerId);
        if (isInModal) {
            updateModalPreview(index);
        } else {
            updateMacroPreview(index);
        }
        if (!isInModal) {
            checkMacroChanges(index);
        }
    }
}

function updateKeyName(index) {
    const nameInput = document.getElementById(`keyName${index}`);
    const titleElement = document.getElementById(`keyTitle${index}`);

    if (nameInput && titleElement) {
        const newName = nameInput.value.trim();
        customKeyNames[index] = newName || null;
        titleElement.textContent = `Key ${index + 1}${newName ? ': ' + newName : ''}`;
    }
}

window.updateKeyName = updateKeyName;


function updateMacroPreview(index) {
    const type = document.getElementById(`macroType${index}`).value;
    const preview = document.getElementById(`macroPreview${index}`);

    if (type === '0') {
        preview.innerHTML = '<p class="preview-text disabled">No macro configured</p>';
    } else if (type === '4') {
        updateSequencePreview(index);
        return;
    }
}

function generateLEDConfig() {
    const config = document.getElementById('ledConfig');
    config.innerHTML = '';

    for (let i = 0; i < 5; i++) {
        const item = document.createElement('div');
        item.className = 'led-item';
        item.innerHTML = `
            <h3>LED ${i}</h3>
            <input type="color" id="ledColor${i}" value="#000032">
            <button onclick="setLEDColor(${i})" style="margin-top: 10px;">Set Color</button>
        `;
        config.appendChild(item);
    }
}

async function connectSerial() {
    try {
        if ('serial' in navigator) {
            // Clean up any stale connection first (fixes "port is already open" on reconnect)
            if (port || reader || writer) {
                await disconnectSerial();
            }

            // Prefer Espressif (0x303A) or Adafruit (0x239A) devices so the
            // browser highlights the macropad in the port picker.  If neither
            // VID matches, the user can still pick any port manually.
            const usbFilters = [
                { usbVendorId: 0x303A },  // Espressif ESP32-S3
                { usbVendorId: 0x239A }   // Adafruit
            ];

            let selectedPort;
            try {
                selectedPort = await navigator.serial.requestPort({ filters: usbFilters });
            } catch (filterErr) {
                // User cancelled or no matching device -- retry without filter
                log('No ESP32-S3 device selected, showing all ports...', 'info');
                selectedPort = await navigator.serial.requestPort();
            }

            port = selectedPort;
            await port.open({ baudRate: 115200 });

            writer = port.writable.getWriter();
            reader = port.readable.getReader();

            isConnected = true;
            updateConnectionStatus(true);
            log('Connected to macropad!', 'success');

            // Start reading
            readSerial();

            document.getElementById('connectBtn').disabled = true;
            document.getElementById('disconnectBtn').disabled = false;

            // Auto-fetch current config from the device
            setTimeout(() => sendCommand('LIST'), 500);
        } else {
            log('Web Serial API not supported. Use Chrome/Edge.', 'error');
        }
    } catch (err) {
        log('Connection error: ' + err.message, 'error');
    }
}

async function disconnectSerial() {
    if (!port) {
        log('Already disconnected', 'info');
        return;
    }

    isConnected = false;
    updateConnectionStatus(false);

    try {
        if (reader) {
            try {
                await reader.cancel();
            } catch (e) { /* ignore */ }
            try {
                reader.releaseLock();
            } catch (e) { /* ignore */ }
            reader = null;
        }

        if (writer) {
            try {
                await writer.close();
            } catch (e) { /* ignore */ }
            try {
                writer.releaseLock();
            } catch (e) { /* ignore */ }
            writer = null;
        }

        try {
            await port.close();
        } catch (e) { /* ignore */ }
        port = null;

    } catch (err) {
        log('Disconnect error: ' + err.message, 'error');
    } finally {
        serialBuffer = '';
        listParsingState = 'idle';
        log('Disconnected', 'info');
        const connectBtn = document.getElementById('connectBtn');
        const disconnectBtn = document.getElementById('disconnectBtn');
        if (connectBtn) connectBtn.disabled = false;
        if (disconnectBtn) disconnectBtn.disabled = true;
    }
}

let serialBuffer = '';

async function readSerial() {
    try {
        while (isConnected && reader) {
            const { value, done } = await reader.read();
            if (done) break;

            serialBuffer += new TextDecoder().decode(value);
            const lines = serialBuffer.split('\n');
            // Keep the last (possibly incomplete) chunk in the buffer
            serialBuffer = lines.pop();

            for (const line of lines) {
                const trimmed = line.trim();
                if (trimmed) {
                    if (parseListOutput(trimmed)) {
                        continue;
                    }
                    log('Device: ' + trimmed, 'info');
                }
            }
        }
    } catch (err) {
        if (isConnected) {
            log('Read error: ' + err.message, 'error');
        }
    }
}

async function sendCommand(cmd) {
    if (!isConnected || !writer) {
        log('Not connected!', 'error');
        return;
    }

    try {
        const data = new TextEncoder().encode(cmd + '\n');
        await writer.write(data);
        log('Sent: ' + cmd, 'info');
    } catch (err) {
        log('Send error: ' + err.message, 'error');
    }
}

function updateConnectionStatus(connected) {
    const status = document.getElementById('connectionStatus');
    if (connected) {
        status.className = 'connection-status status-connected';
        status.textContent = '✅ Connected';
    } else {
        status.className = 'connection-status status-disconnected';
        status.textContent = '⚠️ Not Connected';
    }

    const configSections = ['sectionMacros', 'sectionLEDs', 'sectionOS', 'sectionActions'];
    configSections.forEach(id => {
        const el = document.getElementById(id);
        if (el) el.style.display = connected ? '' : 'none';
    });
}

function log(message, type = 'info') {
    const logDiv = document.getElementById('log');
    const entry = document.createElement('div');
    entry.className = 'log-entry ' + type;
    entry.textContent = '[' + new Date().toLocaleTimeString() + '] ' + message;
    logDiv.appendChild(entry);
    logDiv.scrollTop = logDiv.scrollHeight;
}

function clearLog() {
    document.getElementById('log').innerHTML = '';
}

// Macro functions
async function saveMacro(keyNum) {
    const idx = keyNum - 1;
    const type = document.getElementById(`macroType${idx}`).value;

    // Save key name if changed
    const nameInput = document.getElementById(`keyName${idx}`);
    if (nameInput) {
        const currentName = nameInput.value.trim();
        const originalName = originalMacros[idx]?.keyName || '';
        if (currentName !== originalName && currentName.length > 0) {
            const nameCmd = `NAME ${keyNum} ${currentName}`;
            await sendCommand(nameCmd);
            customKeyNames[idx] = currentName;
            originalMacros[idx].keyName = currentName;
            log(`Key ${keyNum} renamed to: ${currentName}`, 'success');
        }
    }

    if (type === '4') {
        // Sequence type - use new SEQUENCE command
        await saveSequenceMacro(keyNum, idx);
        // Update original state after saving
        originalMacros[idx] = {
            ...originalMacros[idx],
            type: 4,
            sequenceSteps: JSON.parse(JSON.stringify(sequenceSteps[idx] || { windows: [], mac: [] }))
        };
        checkMacroChanges(idx);
        return;
    }

    await sendCommand(`MACRO ${keyNum} ${type} 0 0`);
    log(`Key ${keyNum} macro updated (type=${type})`, 'success');

    originalMacros[idx] = {
        ...originalMacros[idx],
        type: parseInt(type),
        modifier: 0,
        keycode: 0,
        data: ''
    };
    checkMacroChanges(idx);
}

async function saveSequenceMacro(keyNum, idx) {
    if (!sequenceSteps[idx]) {
        sequenceSteps[idx] = { windows: [], mac: [] };
    }

    // Set the macro type to SEQUENCE (4) in EEPROM so the firmware
    // knows to look up the stored sequence data for this key.
    await sendCommand(`MACRO ${keyNum} 4 0 0`);

    // Save sequences for both OS if they exist
    const osTypes = ['windows', 'mac'];

    for (const os of osTypes) {
        const steps = sequenceSteps[idx][os];
        if (!steps || steps.length === 0) continue;

        // Build sequence string
        // Format: SEQUENCE <key> <os> <step1>|<step2>|...
        // Step format: K:modifier:keycode, T:text, D:delay, R:release
        const stepStrings = [];

        steps.forEach(step => {
            if (step.type === 'key') {
                const modifier = step.modifiers ? step.modifiers.reduce((a, b) => a + b, 0) : 0;
                // Handle multiple keys - join them with commas for simultaneous press
                const keys = step.keys && Array.isArray(step.keys) && step.keys.length > 0
                    ? step.keys.filter(k => k && k !== 0).join(',')
                    : (step.keycode && step.keycode !== 0 ? step.keycode : '0');
                stepStrings.push(`K:${modifier}:${keys}`);
            } else if (step.type === 'text') {
                stepStrings.push(`T:${step.text || ''}`);
            } else if (step.type === 'delay') {
                stepStrings.push(`D:${step.delay || 200}`);
            } else if (step.type === 'release') {
                stepStrings.push('R');
            }
        });

        const sequenceStr = stepStrings.join('|');
        const osShort = os === 'windows' ? 'win' : 'mac';
        const cmd = `SEQUENCE ${keyNum} ${osShort} ${sequenceStr}`;
        await sendCommand(cmd);
        log(`Sequence macro ${keyNum} (${os}) saved: ${steps.length} steps`, 'success');
    }
}

// LED functions
async function setLEDColor(index) {
    const colorInput = document.getElementById(`ledColor${index}`);
    const color = colorInput.value;
    const r = parseInt(color.substr(1, 2), 16);
    const g = parseInt(color.substr(3, 2), 16);
    const b = parseInt(color.substr(5, 2), 16);

    const cmd = `LED ${index} ${r} ${g} ${b}`;
    await sendCommand(cmd);
}

async function setBrightness() {
    const brightness = document.getElementById('brightnessSlider').value;
    const cmd = `BRIGHTNESS ${brightness}`;
    await sendCommand(cmd);
}

// OS functions
async function selectOS(os) {
    document.getElementById('osMac').classList.remove('selected');
    document.getElementById('osWin').classList.remove('selected');

    if (os === 'mac') {
        document.getElementById('osMac').classList.add('selected');
    } else {
        document.getElementById('osWin').classList.add('selected');
    }

    const cmd = `OS ${os}`;
    await sendCommand(cmd);
}

// Utility functions
async function listSettings() {
    await sendCommand('LIST');
}

async function refreshSettings() {
    await sendCommand('LIST');
}

async function resetDefaults() {
    if (confirm('Are you sure you want to reset all settings to defaults?')) {
        await sendCommand('RESET');
    }
}

function exportConfig() {
    log('Export functionality - coming soon!', 'info');
}

// Close modal when clicking outside of it
window.onclick = function (event) {
    const modal = document.getElementById('sequenceModal');
    if (event.target == modal) {
        closeSequenceModal();
    }
}

// Close modal with ESC key
document.addEventListener('keydown', function (event) {
    if (event.key === 'Escape') {
        const modal = document.getElementById('sequenceModal');
        if (modal && modal.style.display === 'block') {
            closeSequenceModal();
        }
    }
});

// Parse LIST command output
let listParsingState = 'idle'; // 'idle', 'macros', 'sequences', 'done'
let currentMacroIndex = -1;
let deviceSequences = {}; // Temporary storage for parsed sequence data

function parseListOutput(line) {
    if (line.includes('=== MACRO CONFIGURATION ===')) {
        listParsingState = 'macros';
        loadedMacros = new Array(12).fill(null);
        deviceSequences = {};
        currentMacroIndex = -1;
        return true;
    }

    if (line.includes('=== SEQUENCES ===')) {
        listParsingState = 'sequences';
        return true;
    }

    if (line.includes('=== LED COLORS ===')) {
        if (listParsingState === 'macros' || listParsingState === 'sequences') {
            updateUIWithLoadedMacros();
        }
        listParsingState = 'leds';
        return true;
    }

    // Parse LED lines: "LED 0: RGB(0, 255, 0)"
    if (listParsingState === 'leds') {
        const ledMatch = line.match(/^LED\s+(\d+):\s*RGB\((\d+),\s*(\d+),\s*(\d+)\)/);
        if (ledMatch) {
            const idx = parseInt(ledMatch[1]);
            const r = parseInt(ledMatch[2]);
            const g = parseInt(ledMatch[3]);
            const b = parseInt(ledMatch[4]);
            const hex = '#' + [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
            const colorInput = document.getElementById(`ledColor${idx}`);
            if (colorInput) colorInput.value = hex;
            return true;
        }
    }

    // Parse OS Mode and Brightness
    if (line.startsWith('OS Mode:')) {
        listParsingState = 'done';
        const os = line.includes('macOS') ? 'mac' : 'win';
        document.getElementById('osMac')?.classList.toggle('selected', os === 'mac');
        document.getElementById('osWin')?.classList.toggle('selected', os === 'win');
        return true;
    }

    if (line.startsWith('LED Brightness:')) {
        listParsingState = 'done';
        const val = parseInt(line.split(':')[1]) || 50;
        const slider = document.getElementById('brightnessSlider');
        const label = document.getElementById('brightnessValue');
        if (slider) slider.value = val;
        if (label) label.textContent = val;
        return true;
    }

    // Parse sequence lines: "9:win:K:8:21|T:cursor"
    if (listParsingState === 'sequences') {
        const colonIdx = line.indexOf(':');
        if (colonIdx > 0) {
            const keyNum = parseInt(line.substring(0, colonIdx));
            if (!isNaN(keyNum) && keyNum >= 1 && keyNum <= 12) {
                const rest = line.substring(colonIdx + 1);
                const osIdx = rest.indexOf(':');
                if (osIdx > 0) {
                    const os = rest.substring(0, osIdx);  // "win" or "mac"
                    const seqStr = rest.substring(osIdx + 1);
                    const idx = keyNum - 1;
                    if (!deviceSequences[idx]) deviceSequences[idx] = {};
                    deviceSequences[idx][os === 'mac' ? 'mac' : 'windows'] = parseSequenceString(seqStr);
                }
            }
        }
        return true;
    }

    // Parse macro table rows
    if (listParsingState === 'macros' && line.includes('|')) {
        const parts = line.split('|').map(p => p.trim());
        if (parts.length >= 6 && !isNaN(parts[0])) {
            const keyNum = parseInt(parts[0]) - 1;
            const keyName = parts[1].trim();
            const typeStr = parts[2].trim();
            const modifier = parseInt(parts[3]) || 0;
            const keycode = parseInt(parts[4]) || 0;
            const data = parts[5] || '';

            let type = 0;
            if (typeStr.includes('KEYCOMBO')) type = 1;
            else if (typeStr.includes('STRING')) type = 2;
            else if (typeStr.includes('SPECIAL')) type = 3;
            else if (typeStr.includes('SEQUENCE')) type = 4;

            loadedMacros[keyNum] = { type, modifier, keycode, data };

            if (keyName && keyName.length > 0) {
                customKeyNames[keyNum] = keyName;
            }

            return true;
        } else if (parts.length >= 5 && !isNaN(parts[0])) {
            const keyNum = parseInt(parts[0]) - 1;
            const typeStr = parts[1].trim();
            const modifier = parseInt(parts[2]) || 0;
            const keycode = parseInt(parts[3]) || 0;
            const data = parts[4] || '';

            let type = 0;
            if (typeStr.includes('KEYCOMBO')) type = 1;
            else if (typeStr.includes('STRING')) type = 2;
            else if (typeStr.includes('SPECIAL')) type = 3;
            else if (typeStr.includes('SEQUENCE')) type = 4;

            loadedMacros[keyNum] = { type, modifier, keycode, data };
            return true;
        }
    }

    return false;
}

// Convert a device sequence string like "K:8:21|T:cursor|D:200"
// into the UI step objects that sequenceSteps uses.
function parseSequenceString(seqStr) {
    if (!seqStr || seqStr.length === 0) return [];
    const parts = seqStr.split('|');
    let idCounter = Date.now();
    return parts.map(part => {
        const step = { id: idCounter++ };
        if (part.startsWith('K:')) {
            step.type = 'key';
            const kParts = part.substring(2).split(':');
            const mod = parseInt(kParts[0]) || 0;
            step.modifiers = [];
            if (mod & 1) step.modifiers.push(1);
            if (mod & 2) step.modifiers.push(2);
            if (mod & 4) step.modifiers.push(4);
            if (mod & 8) step.modifiers.push(8);
            const keyStr = kParts[1] || '0';
            step.keys = keyStr.split(',').map(k => parseInt(k) || 0);
            step.keycode = step.keys[0];
        } else if (part.startsWith('T:')) {
            step.type = 'text';
            step.text = part.substring(2);
        } else if (part.startsWith('D:')) {
            step.type = 'delay';
            step.delay = parseInt(part.substring(2)) || 200;
        } else if (part === 'R') {
            step.type = 'release';
        }
        return step;
    }).filter(s => s.type);
}

function updateUIWithLoadedMacros() {
    for (let i = 0; i < 12; i++) {
        if (loadedMacros[i]) {
            const macro = loadedMacros[i];
            const typeSelect = document.getElementById(`macroType${i}`);

            if (typeSelect) {
                // Map device types that aren't in the dropdown to sequence (4)
                // so the UI can display them. Types 1 (KEYCOMBO) and 3 (SPECIAL)
                // are single-step sequences in the UI's model.
                let uiType = macro.type;
                if (uiType === 1 || uiType === 2 || uiType === 3) {
                    uiType = 4;
                }
                typeSelect.value = uiType;
                updateMacroUI(i);

                // Apply device sequence data if available
                if (deviceSequences[i]) {
                    sequenceSteps[i] = {
                        windows: deviceSequences[i].windows || sequenceSteps[i]?.windows || [],
                        mac: deviceSequences[i].mac || sequenceSteps[i]?.mac || []
                    };
                }

                // Update key name (from device only, no defaults)
                const nameInput = document.getElementById(`keyName${i}`);
                const titleElement = document.getElementById(`keyTitle${i}`);
                const keyName = customKeyNames[i] ?? macro.keyName ?? '';
                if (nameInput) nameInput.value = keyName;
                if (titleElement) titleElement.textContent = `Key ${i + 1}${keyName ? ': ' + keyName : ''}`;

                // Update original state so Save button reflects clean state
                originalMacros[i] = JSON.parse(JSON.stringify(macro));
                originalMacros[i].type = uiType;
                if (!originalMacros[i].keyName) {
                    originalMacros[i].keyName = keyName;
                }
                originalMacros[i].sequenceSteps = JSON.parse(JSON.stringify(sequenceSteps[i] || { windows: [], mac: [] }));

                updateMacroPreview(i);
                checkMacroChanges(i);
            }
        }
    }
    const loadedCount = loadedMacros.filter(m => m !== null).length;
    log(`Loaded ${loadedCount} macros from device`, 'success');
}

// Initialize on load
window.addEventListener('load', () => {
    initUI();
    // Initialize previews for all macros and check for changes
    for (let i = 0; i < 12; i++) {
        updateMacroPreview(i);
        checkMacroChanges(i);
    }
    // Explicit disconnect handler (onclick can be unreliable with async)
    document.getElementById('disconnectBtn')?.addEventListener('click', () => disconnectSerial());
    log('Configurator ready. Connect to your macropad and click "Refresh Settings" to load existing macros.', 'info');
});

