WaTTrack — Home Energy Auditor
================================

HOW TO RUN
----------
1. Keep "WaTTrack.exe" and the "dist" folder together in the same
   folder (don't separate them — the app reads its files from "dist").
2. Double-click "WaTTrack.exe".
3. A black console window opens and your default browser opens
   automatically to the app. If the browser doesn't open by itself,
   go to: http://localhost:3000

4. To close the app, close the black console window.


ABOUT THE "WINDOWS PROTECTED YOUR PC" WARNING
----------------------------------------------
Because this .exe wasn't signed with a paid Windows code-signing
certificate, Windows SmartScreen may show a blue warning screen the
first time you run it. This is normal for independently-built software,
not a sign that anything is wrong. To proceed:

  Click "More info"  ->  then click "Run anyway"

Windows only asks this once per file.


YOUR SAVED AUDITS
-------------------
WaTTrack saves your projects, devices, and settings directly in the
browser (no cloud, no account). This program always tries to start on
the same address (http://localhost:3000) every time specifically so
your saved data stays reachable from one session to the next. It only
falls back to a different port in the rare case something else on
your PC is already using 3000 — if that ever happens, older data saved
under a different port won't show up until you're back on the same
port. In practice this is very unlikely to affect you.


INTERNET CONNECTION
--------------------
All the auditing, charts, the upgrade simulator, and PDF export run
locally in the browser — no internet connection is required for
day-to-day use. The one exception is the app's custom fonts (Space
Grotesk, JetBrains Mono), which are loaded from Google Fonts the first
time; without internet the app still works perfectly, just with your
browser's default fonts instead.


TROUBLESHOOTING
----------------
- "Could not find a free port": something else on your PC is using
  ports 3000–3019. Close the other app, or restart your PC, then
  try again.
- Nothing happens when you double-click: right-click WaTTrack.exe ->
  "Run as administrator" isn't normally required, but try it once if
  Windows is blocking it silently.
- Antivirus flags it: this can happen with any unsigned .exe built
  outside an app store. The full source code is in the "source"
  folder if you'd like to inspect it or rebuild it yourself
  (see source/BUILD.txt).


WHAT THIS EXE ACTUALLY IS
---------------------------
A small native Windows program (no Node.js, no installer, no
dependencies) that serves the WaTTrack web app on your machine and
opens it in your browser. All of the energy calculations, charts, and
PDF export run locally in the browser exactly as they do in the
original web app.
