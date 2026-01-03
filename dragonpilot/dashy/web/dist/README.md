# dashy - dragonpilot's All-in-one System Hub for You

A modern web-based dashboard for monitoring and controlling your dragonpilot/openpilot remotely.

## Usage

These files should be served by the Dashy backend server running on your comma device.

Access the interface by navigating to `http://<comma-device-ip>:5088` in Chrome browser.

## Files

- `index.html` - Main application
- `js/app.js` - Minified JavaScript bundle
- `css/styles.css` - Minified styles
- `icons/` - Favicon
- `pages/player.html` - HLS video player

## Features

- **Live Driving View** - Real-time WebRTC video with augmented reality overlay
- **File Browser** - Access and stream driving recordings
- **Settings Control** - Configure vehicle and display preferences

## 🎮 Usage

### Navigation
- **Driving View** - Live camera feed with lane lines and path visualization
- **Files** - Browse `/data/media/0/realdata` recordings
- **Local Settings** - Adjust display preferences

### Display Options
- Metric/Imperial units
- HUD mode for windshield projection

## Requirements

- dragonpilot/openpilot device (comma 3/3X)
- Chrome browser (recommended)
- Same network connection as vehicle

## 📄 License

Copyright (c) 2025, Rick Lan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, and/or sublicense, 
for non-commercial purposes only, subject to the following conditions:

- The above copyright notice and this permission notice shall be included in 
  all copies or substantial portions of the Software.
- Commercial use (e.g. use in a product, service, or activity intended to 
  generate revenue) is prohibited without explicit written permission from 
  the copyright holder.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
