# WaTTrack Desktop — Home Energy Auditor (Windows Edition)

**WaTTrack Desktop** brings this professional-grade, high-density household energy auditing application straight to your local workstation. Packaged as a lightweight, zero-dependency Windows executable (`.exe`), it offers a local-first environment for homeowners, auditors, and inspectors to catalog appliances, isolate utility hogs, and run upgrade simulations with total data privacy.

The application preserves its signature **Industrial Slate Theme**, combining structural geometry (`Space Grotesk` & `JetBrains Mono`) with a high-contrast, fatigue-free paper aesthetic designed to replicate physical engineering worksheets.

---

## 🌟 Desktop-Specific Features

* **Zero Dependencies:** Runs natively on Windows without requiring Node.js, runtime libraries, or complex local databases.
* **Local-First Data Privacy:** Your audit registries, appliance lists, and property parameters are stored directly within your local browser storage. No cloud accounts, no third-party tracking, and absolute data sovereignty.
* **Persistent Session Ports:** The launcher locks onto `http://localhost:3000` deliberately to ensure your web browser can instantly pull your saved local data across multiple application launches.
* **Full Simulation Framework:** Toggle retroactive LED swaps, smart thermostat profiles, and standby vampire load filters completely offline to instantly forecast physical ($kWh$) and financial utility savings.

---

## 📁 PC Distribution Structure

For proper runtime local hosting, ensure that the executable engine and its distribution directory stay in the same parent folder:

```text
├── WaTTrack.exe          # Native Windows background server launcher
├── dist/                 # Local distribution folder containing compiled assets
│   ├── index.html        # Primary graphical user interface entry point
│   └── assets/           # Bundled energy audit logic, upgrade simulator, and charts
└── README.txt            # Quick-start manual, persistence notes, and troubleshooting

```

---

## 🚀 Quick Start Guide

### 1. Launching the App

1. Download and extract the full package. Ensure `WaTTrack.exe` and the `dist` folder reside together.
2. Double-click **`WaTTrack.exe`**.
3. A black command console will initialize, and your default web browser will automatically open the dashboard at: `http://localhost:3000`.

> 💡 *If the browser does not open automatically, open any modern browser and navigate manually to `http://localhost:3000`.*

### 2. Bypassing Windows SmartScreen

Because this independent utility is compiled outside the Microsoft Store without a premium commercial code-signing certificate, Windows SmartScreen may show a blue warning screen upon its first run.

* Click **"More info"**
* Click **"Run anyway"**
*(Windows will cache this permission immediately and will not prompt you again for this release).*

### 3. Closing the Application

To shut down the background server and safely release the networking port, simply **close the black console window**.

---

## 🛠️ Troubleshooting & Data Persistence

* **Saved Audits Visibility:** WaTTrack links your data history to your local network port address. In the rare event that port `3000` is blocked and the app falls back to an alternative port (up to `3019`), older data will temporarily disappear until port `3000` is freed up and reopened.
* **Port Conflicts (`Could not find a free port`):** If another app or development tool is hogging ports `3000–3019`, clear the conflicting process or reboot your workstation.
* **Offline Typography:** The application runs 100% offline. If you use it entirely disconnected from the internet, it will safely fall back to standard system sans-serif/monospace fonts without affecting any underlying auditing calculations or PDF exports.
