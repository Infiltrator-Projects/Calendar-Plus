// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

/*
 * Small Cinnamon/GJS compatibility primitives shared by Calendar Plus views.
 *
 * This module owns only runtime mechanics that are independent of any calendar
 * feature: local-module resolution across Cinnamon 6.4/6.6/6.7 and deterministic
 * signal ownership.  Keeping these primitives here prevents each view from
 * growing its own slightly different teardown implementation.
 */

function loadLocalModule(name) {
    try {
        const Extension = imports.ui.extension;
        if (Extension && typeof Extension.getCurrentExtension === "function") {
            const extension = Extension.getCurrentExtension();
            if (extension && extension.imports && extension.imports[name]) {
                return extension.imports[name];
            }
        }
    } catch (error) {
        /* Fall through when Cinnamon's current-extension lookup is unavailable. */
    }

    return require(`./${name}`);
}

var SignalBag = class SignalBag {
    constructor() {
        this._connections = [];
    }

    connect(object, signal, callback) {
        if (!object || typeof object.connect !== "function") {
            return 0;
        }

        const id = object.connect(signal, callback);
        this._connections.push([object, id]);
        return id;
    }

    disconnectAll() {
        for (const [object, id] of this._connections.splice(0)) {
            if (!object || id <= 0 || typeof object.disconnect !== "function") {
                continue;
            }
            try {
                object.disconnect(id);
            } catch (error) {
                global.logError(error);
            }
        }
    }
};
