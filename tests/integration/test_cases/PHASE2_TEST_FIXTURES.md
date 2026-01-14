# Phase 2 HTML Verbs - Test Fixture Specifications

This document describes the test image fixtures needed for Phase 2 HTML verb integration tests.

## Overview

Phase 2 includes image analysis verbs (`html.getgifheightwidth` and `html.getjpegheightwidth`) that require sample image files for testing. These images will be stored in the test fixture directory and referenced using the `{FRONTIER_TEST_TMP_DIR}` template in test scripts.

## Required Test Images

### GIF Files (7 images)

1. **sample_100x50.gif**
   - Dimensions: 100 pixels wide × 50 pixels high
   - Purpose: Basic landscape-oriented GIF test
   - Format: GIF87a or GIF89a

2. **sample_200x200.gif**
   - Dimensions: 200 × 200 pixels (square)
   - Purpose: Square aspect ratio test
   - Format: GIF87a or GIF89a

3. **sample_1x1.gif**
   - Dimensions: 1 × 1 pixel
   - Purpose: Minimum size edge case
   - Format: GIF87a or GIF89a
   - Note: Common "tracking pixel" size

4. **sample_1024x768.gif**
   - Dimensions: 1024 × 768 pixels
   - Purpose: Large image test
   - Format: GIF87a or GIF89a

5. **empty.gif**
   - Size: 0 bytes (empty file)
   - Purpose: Test error handling for empty files

6. **corrupt.gif**
   - Contents: File starting with "GIF" but invalid header data
   - Purpose: Test error handling for corrupted files

7. **actually_jpeg.gif**
   - Contents: JPEG file with .gif extension
   - Purpose: Test error handling for wrong file type

### JPEG Files (7 images)

1. **sample_100x50.jpg**
   - Dimensions: 100 × 50 pixels
   - Purpose: Basic landscape-oriented JPEG test
   - Format: Standard JPEG/JFIF

2. **sample_200x200.jpg**
   - Dimensions: 200 × 200 pixels (square)
   - Purpose: Square aspect ratio test
   - Format: Standard JPEG/JFIF

3. **sample_1920x1080.jpg**
   - Dimensions: 1920 × 1080 pixels
   - Purpose: HD resolution test
   - Format: Standard JPEG/JFIF

4. **sample_600x800.jpg**
   - Dimensions: 600 × 800 pixels
   - Purpose: Portrait orientation test
   - Format: Standard JPEG/JFIF

5. **sample_progressive.jpg**
   - Dimensions: Any reasonable size (e.g., 400 × 300)
   - Purpose: Test progressive JPEG support
   - Format: Progressive JPEG

6. **empty.jpg**
   - Size: 0 bytes (empty file)
   - Purpose: Test error handling for empty files

7. **corrupt.jpg**
   - Contents: File starting with JPEG magic bytes (FF D8) but invalid data
   - Purpose: Test error handling for corrupted files

8. **actually_gif.jpg**
   - Contents: GIF file with .jpg extension
   - Purpose: Test error handling for wrong file type

## Image Creation Guide

### Creating Test Images

You can create these test images using various tools:

**Command-line (ImageMagick)**:
```bash
# Create GIF files
convert -size 100x50 xc:blue sample_100x50.gif
convert -size 200x200 xc:green sample_200x200.gif
convert -size 1x1 xc:red sample_1x1.gif
convert -size 1024x768 xc:yellow sample_1024x768.gif

# Create JPEG files
convert -size 100x50 xc:blue sample_100x50.jpg
convert -size 200x200 xc:green sample_200x200.jpg
convert -size 1920x1080 xc:red sample_1920x1080.jpg
convert -size 600x800 xc:yellow sample_600x800.jpg
convert -size 400x300 xc:cyan -interlace Plane sample_progressive.jpg

# Create corrupted files
echo "GIF89a\x00\x00" > corrupt.gif
echo -n "\xFF\xD8\xFF" > corrupt.jpg

# Create misnamed files
cp sample_100x50.jpg actually_jpeg.gif
cp sample_100x50.gif actually_gif.jpg

# Create empty files
touch empty.gif
touch empty.jpg
```

**Python (PIL/Pillow)**:
```python
from PIL import Image

# Create GIF files
Image.new('RGB', (100, 50), 'blue').save('sample_100x50.gif')
Image.new('RGB', (200, 200), 'green').save('sample_200x200.gif')
Image.new('RGB', (1, 1), 'red').save('sample_1x1.gif')
Image.new('RGB', (1024, 768), 'yellow').save('sample_1024x768.gif')

# Create JPEG files
Image.new('RGB', (100, 50), 'blue').save('sample_100x50.jpg')
Image.new('RGB', (200, 200), 'green').save('sample_200x200.jpg')
Image.new('RGB', (1920, 1080), 'red').save('sample_1920x1080.jpg')
Image.new('RGB', (600, 800), 'yellow').save('sample_600x800.jpg')

# Create progressive JPEG
img = Image.new('RGB', (400, 300), 'cyan')
img.save('sample_progressive.jpg', 'JPEG', progressive=True)
```

## Fixture Directory Structure

```
tests/fixtures/images/
├── gif/
│   ├── sample_100x50.gif
│   ├── sample_200x200.gif
│   ├── sample_1x1.gif
│   ├── sample_1024x768.gif
│   ├── empty.gif
│   ├── corrupt.gif
│   └── actually_jpeg.gif
└── jpeg/
    ├── sample_100x50.jpg
    ├── sample_200x200.jpg
    ├── sample_1920x1080.jpg
    ├── sample_600x800.jpg
    ├── sample_progressive.jpg
    ├── empty.jpg
    ├── corrupt.jpg
    └── actually_gif.jpg
```

## Test Script Usage

In integration tests, reference these images using the `{FRONTIER_TEST_TMP_DIR}` template:

```yaml
- name: "html.getgifheightwidth - basic GIF file"
  script: |
    local(testDir = "{FRONTIER_TEST_TMP_DIR}");
    local(gifPath = testDir + "/sample_100x50.gif");
    local(dimensions = html.getgifheightwidth(gifPath));
    return (dimensions[1] == 50) and (dimensions[2] == 100)
  expected_result: "true"
```

The integration test framework will:
1. Copy fixtures from `tests/fixtures/images/` to a temporary directory
2. Replace `{FRONTIER_TEST_TMP_DIR}` with the absolute path to that directory
3. Run the test with proper file paths (avoiding macOS sandbox /tmp restriction)

## Validation Checklist

Before using fixtures in tests:

- [ ] All GIF files are valid GIF87a or GIF89a format
- [ ] All JPEG files are valid JFIF format
- [ ] Dimensions match filename specifications
- [ ] Progressive JPEG is correctly encoded
- [ ] Corrupted files have valid magic bytes but invalid headers
- [ ] Misnamed files are actually the wrong format
- [ ] Empty files are exactly 0 bytes
- [ ] All files are committed to git with proper .gitattributes (binary)

## Test Execution

To run Phase 2 image tests:

```bash
# Run all Phase 2 tests
cd tests && make test-integration FILTER=html_image_tests

# Run with verbose output
cd tests && make test-integration FILTER=html_image_tests VERBOSE=1

# Create fixtures first (if needed)
cd tests/fixtures && python3 create_image_fixtures.py
```

## Notes

1. **File Sizes**: Keep test images small (< 100KB each) to avoid bloating the repository
2. **Colors**: Use solid colors for simplicity (no complex patterns needed)
3. **Formats**: Stick to standard formats (no exotic compression or encoding)
4. **Permissions**: Ensure files are readable by frontier-cli process
5. **Binary Files**: Add `*.gif binary` and `*.jpg binary` to `.gitattributes`

## Future Enhancements

Additional fixtures that could be added in the future:

- Animated GIF (multi-frame)
- JPEG with EXIF orientation data
- Interlaced GIF
- Very large images (test performance limits)
- Images with unusual bit depths
- Grayscale images
- Images with transparency (GIF transparent color)

---

**Created**: 2026-01-14
**Purpose**: Document test fixtures for Phase 2 HTML verb integration tests
**Status**: Fixtures need to be created before tests can run
