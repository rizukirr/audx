# Legal Notices and Licensing Information

## audx License

The audx source code (all `.c`, `.h`, and `CMakeLists.txt` files in this repository) is licensed under the **MIT License**. See the [LICENSE](LICENSE) file for the full text.

---

## FFmpeg Library Usage and LGPL Compliance

### Overview

audx dynamically links to FFmpeg libraries to provide audio decoding, encoding, and filtering functionality. FFmpeg is licensed under the **GNU Lesser General Public License (LGPL) version 2.1 or later**.

### FFmpeg Libraries Used

This software uses the following FFmpeg libraries:
- **libavcodec** - Audio/video codec library
- **libavformat** - Container format demuxing/muxing library
- **libavutil** - Utility library (common functions, mathematics, etc.)
- **libavfilter** - Audio/video filtering library
- **libswresample** - Audio resampling library
- **libswscale** - Video scaling library

All of these libraries are part of the FFmpeg project: https://ffmpeg.org/

### LGPL Compliance Statement

audx complies with the LGPL requirements as follows:

1. **Dynamic Linking**: audx dynamically links to FFmpeg libraries (shared libraries / `.so` files on Linux, `.dylib` on macOS, `.dll` on Windows). This allows users to replace FFmpeg libraries with their own versions.

2. **No Modifications**: audx does not modify FFmpeg source code. It uses the standard FFmpeg API as provided by the system or user-installed FFmpeg libraries.

3. **Source Code Availability**: FFmpeg source code is available at:
   - Official website: https://ffmpeg.org/download.html
   - Git repository: https://git.ffmpeg.org/ffmpeg.git
   - GitHub mirror: https://github.com/FFmpeg/FFmpeg

4. **License Text**: The complete LGPL 2.1 license text is available at:
   - https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
   - https://www.gnu.org/licenses/lgpl-2.1.txt

### FFmpeg Version Information

To check which version of FFmpeg libraries audx is using on your system:
```bash
audx --version
```

Or check your system's FFmpeg installation:
```bash
ffmpeg -version
```

### Obtaining FFmpeg Source Code

If you need the FFmpeg source code corresponding to your installed libraries:

**On Arch Linux:**
```bash
# Get source for installed version
asp export ffmpeg
cd ffmpeg
makepkg --nobuild
```

**On Debian/Ubuntu:**
```bash
# Enable source repositories in /etc/apt/sources.list (uncomment deb-src lines)
sudo apt update
apt source ffmpeg
```

**Building from Source:**
```bash
git clone https://git.ffmpeg.org/ffmpeg.git
cd ffmpeg
./configure --enable-shared --disable-static
make
sudo make install
```

---

## GPL Considerations

### Current Status: LGPL ✅

audx uses only LGPL-licensed FFmpeg libraries. The codecs and features enabled are:
- libmp3lame (LGPL)
- AAC (native FFmpeg encoder, LGPL)
- Opus (libopus, BSD-style license)
- FLAC (BSD-style license)
- ALAC (Apache 2.0 license)
- PCM (no licensing restrictions)

### When FFmpeg Becomes GPL ⚠️

FFmpeg can become GPL-licensed if compiled with certain features:
- `--enable-gpl` flag during compilation
- GPL-licensed encoders like **libx264**, **libx265**, **libx264rgb**
- GPL-licensed filters

If you compile or install FFmpeg with GPL components, those components will be GPL-licensed, and derivative works must comply with GPL requirements.

**audx does not enable or require any GPL components.** However, if your system's FFmpeg installation includes GPL components, audx will still function but may be subject to GPL when using those specific codecs.

### How to Check Your FFmpeg Build

```bash
ffmpeg -version | grep configuration
```

Look for `--enable-gpl` in the output. If present, your FFmpeg includes GPL components.

---

## Patent and Codec Licensing

### Important Patent Notices ⚠️

Beyond copyright licensing (MIT/LGPL), certain audio and video codecs may be subject to **patent licensing requirements** in some jurisdictions. This is separate from open source licensing.

### Codecs Used by audx

| Codec | Patent Status | Notes |
|-------|---------------|-------|
| **MP3** (libmp3lame) | Patents expired (as of 2017 in most jurisdictions) | Generally safe to use |
| **AAC** | May require patent licenses for commercial distribution | Consult a lawyer for commercial products |
| **Opus** | Royalty-free, no known patent encumbrances | Safe to use |
| **FLAC** | No patents, open format | Safe to use |
| **ALAC** | Apple codec, royalty-free | Safe to use |
| **PCM/WAV** | No patents (uncompressed audio) | Safe to use |

### Commercial Use Considerations

**If you are distributing audx or derivatives as a commercial product:**

1. **MP3**: Patents have expired worldwide as of 2017, but verify with your legal counsel for your specific jurisdiction.

2. **AAC**: May require licenses from:
   - Via Licensing (https://www.via-la.com/)
   - MPEG LA (https://www.mpegla.com/)

3. **Opus/FLAC/ALAC/PCM**: Generally considered royalty-free.

**Disclaimer**: This is not legal advice. Consult with a qualified intellectual property attorney regarding patent licensing for your specific use case and jurisdiction.

---

## Attribution Requirements

### For Binary Distributions

If you distribute audx binaries, you must:

1. ✅ Include this `LEGAL_NOTICES.md` file
2. ✅ Include the `LICENSE` file
3. ✅ Provide information on how to obtain FFmpeg source code (see above)
4. ✅ Ensure `audx --version` displays FFmpeg library information

### For Modified Versions

If you modify audx source code:

1. ✅ Maintain MIT License notice in source files
2. ✅ Document your changes
3. ✅ Continue to comply with FFmpeg LGPL requirements

If you modify FFmpeg libraries:

1. ⚠️ You must provide source code for your modifications
2. ⚠️ Your modifications must be LGPL-licensed
3. ⚠️ You must clearly document what you changed

---

## User Rights Under LGPL

As a user of audx, the LGPL grants you the following rights regarding FFmpeg libraries:

✅ **Right to Use**: Use audx and FFmpeg libraries for any purpose, including commercial use

✅ **Right to Study**: Examine FFmpeg source code to understand how it works

✅ **Right to Modify**: Modify FFmpeg libraries for your own use

✅ **Right to Replace**: Replace the FFmpeg libraries audx uses with your own version, as long as they maintain API compatibility

✅ **Right to Redistribute**: Distribute audx binaries, as long as you comply with LGPL requirements (provide source code access to FFmpeg)

---

## No Warranty

Both audx (MIT License) and FFmpeg (LGPL) are provided **"AS IS"** without warranty of any kind. See the LICENSE file and LGPL text for complete warranty disclaimers.

---

## Questions and Support

### Legal Questions
For legal questions about FFmpeg licensing, consult:
- FFmpeg legal page: https://ffmpeg.org/legal.html
- Your legal counsel

### Technical Questions
For audx technical questions:
- GitHub Issues: [Your repository URL here]
- FFmpeg documentation: https://ffmpeg.org/documentation.html

---

## References

- FFmpeg Official Website: https://ffmpeg.org/
- FFmpeg Legal Information: https://ffmpeg.org/legal.html
- LGPL 2.1 License: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
- FFmpeg License Documentation: https://ffmpeg.org/doxygen/trunk/md_LICENSE.html
- MIT License: https://opensource.org/licenses/MIT

---

**Last Updated**: 2025-01-17
