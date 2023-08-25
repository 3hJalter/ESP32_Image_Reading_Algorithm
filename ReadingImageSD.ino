#include <SD.h>
#include <TFT_eSPI.h>
#include <JPEGDecoder.h>
#include <SPI.h>
#include <FS.h>

#define sd_cs 5
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

TFT_eSPI tft = TFT_eSPI();
const int chipSelectPin = 5;
const int buttonPin = 0;

// Create a list to store the image file names
String imageFiles[100]; // You can adjust the size of the array based on your needs
int numImages = 0;
int currentImageIndex = 0;

// const int buttonPin = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // Adjust this value based on your button's behavior
int lastButtonState = LOW;
int buttonState = LOW;

void setup() {
  Serial.begin(115200);
  digitalWrite(21, HIGH); // TFT screen chip select
  digitalWrite(chipSelectPin, HIGH); // SD card chips select, must use GPIO 5 (ESP32 SS)
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);

  pinMode(buttonPin, INPUT_PULLUP);
  if (!SD.begin(chipSelectPin)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.println("initialisation done.");

  // Scan the microSD card for image files and store their names in the array
  numImages = scanImages();
  if (numImages == 0) {
    Serial.println("No image files found!");
    return;
  }

  // // Display the first image
  // //displayImage(imageFiles[currentImageIndex]);

  // tft.setRotation(0);
  // tft.fillScreen(0xFFFF);
  // // The image is 240 x 240 pixels so we do some sums to position image in the middle of the screen!
  // // Doing this by reading the image width and height from the jpeg info is left as an exercise!
  // int x = (tft.width()  - 240) / 2 - 1;
  // int y = (tft.height() - 320) / 2 - 1;
  // String fullPath = "/" + imageFiles[0];
  // drawSdJpeg(fullPath, x, y);     // This draws a jpeg pulled off the SD Card

}

void loop() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) {
        Serial.println("Change image");
        delay(200); // Debounce delay

        // Load and display the next image
        currentImageIndex = (currentImageIndex + 1) % numImages;
        tft.setRotation(0);
        tft.fillScreen(random(0xFFFF));
        // int x = 0;
        // int y = 0;
        // The image is 240 x 240 pixels so we do some sums to position image in the middle of the screen!
        // Doing this by reading the image width and height from the jpeg info is left as an exercise!
        int x = (tft.width()  - 240) / 2 ;
        int y = (tft.height() - 320) / 2 ;
        String fullPath = "/" + imageFiles[currentImageIndex];
        drawSdJpeg(fullPath, x, y);     // This draws a jpeg pulled off the SD Card
      }
    }
  }
  lastButtonState = reading;
}

int scanImages() {
  int count = 0;
  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open directory");
    return 0;
  }
  
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      // No more files
      break;
    }
    if (entry.isDirectory()) {
      // Skip directories
      entry.close();
      continue;
    }

    // Check if the file has an image extension
    String filename = entry.name();
    if (filename.endsWith(".jpg") || filename.endsWith(".jpeg") || filename.endsWith(".png") || filename.endsWith(".bmp")) {
      imageFiles[count++] = filename;
      Serial.println(filename);
    }

    entry.close();
  }

  root.close();
  return count;
}

void drawSdJpeg(const String& filename, int xpos, int ypos) {

  // Open the named file (the Jpeg decoder library will close it)
  File jpegFile = SD.open( filename, FILE_READ);  // or, file handle reference for SD library
 
  if ( !jpegFile ) {
    Serial.print("ERROR: File \""); Serial.print(filename); Serial.println ("\" not found!");
    return;
  }

  Serial.println("===========================");
  Serial.print("Drawing file: "); Serial.println(filename);
  Serial.println("===========================");

  // Use one of the following methods to initialise the decoder:
  bool decoded = JpegDec.decodeSdFile(jpegFile);  // Pass the SD file handle to the decoder,
  //bool decoded = JpegDec.decodeSdFile(filename);  // or pass the filename (String or character array)

  if (decoded) {
    // print information about the image to the serial port
    jpegInfo();
    // render the image onto the screen at given coordinates
    jpegRender(xpos, ypos);
  }
  else {
    Serial.println("Jpeg file format not supported!");
  }
}

//####################################################################################################
// Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
//####################################################################################################
// This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
// fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.

void showTime(uint32_t msTime) {
  Serial.print(F(" JPEG drawn in "));
  Serial.print(msTime);
  Serial.println(F(" ms "));
}

bool isPowerOfTwo(float value) {
    if (value <= 0) {
        return false;
    }
    int intValue = static_cast<int>(value);
    return (intValue & (intValue - 1)) == 0;
}

void jpegRender(int xpos, int ypos) {
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;
  bool swapBytes = tft.getSwapBytes();
  tft.setSwapBytes(true);
  uint32_t min_w = jpg_min(mcu_w, max_x % mcu_w);
  uint32_t min_h = jpg_min(mcu_h, max_y % mcu_h);
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;
  uint32_t drawTime = millis();
  max_x += xpos;
  max_y += ypos;

  if (max_x <= 240 && max_y <= 320) {
      handleImageSizeFitScreen(pImg, win_w, win_h, mcu_w, mcu_h, max_x, max_y, min_w, min_h, xpos, ypos);
  } else {
    float scale_x = (float) tft.width() / max_x;
    float scale_y = (float) tft.height() / max_y;
    if (scale_x < scale_y) scale_y = scale_x;
    else scale_x = scale_y;
    Serial.println(scale_x);
    float index = 1.0 / scale_x;
    Serial.println(index);
    // case: scale is 2 to the power of n
    // if (isPowerOfTwo(scale_x)) {
    if (index == static_cast<int>(index) && isPowerOfTwo(index)) {
        Serial.println("Image scale 2 power n: ");
        // Serial.println("Decoding with fit screen ...");
        // handleImageSizeFitScreen(pImg, win_w, win_h, mcu_w, mcu_h, max_x, max_y, min_w, min_h, xpos, ypos);
        // Serial.println("Done!");
        // delay(3000);
        // tft.fillScreen(random(0xFFFF));
        Serial.println("Decoding with bipolar ...");
        handleImageSizeWithBipolarAlgo(pImg, win_w, win_h, mcu_w, mcu_h, max_x, max_y, min_w, min_h, xpos, ypos, scale_x, scale_y);
        Serial.println("Done!");
    // } else if (scale_x == static_cast<int>(scale_x)) {
    } else if (index == static_cast<int>(index)) { 
      Serial.println("Image scale is integer: ");
        // Serial.println("Decoding with bipolar ...");
        // handleImageSizeWithBipolarAlgo(pImg, win_w, win_h, mcu_w, mcu_h, max_x, max_y, min_w, min_h, xpos, ypos, scale_x, scale_y);
        // Serial.println("Done!");
        Serial.println("Decoding with Bicubic ...");
        handleImageSizeWithBicubicAlgo(pImg, win_w, win_h, mcu_w, mcu_h, max_x, max_y, min_w, min_h, xpos, ypos, scale_x, scale_y);
        Serial.println("Done!");
    } else {
        handleImageSizeWithLanczos(pImg, win_w, win_h, mcu_w, mcu_h, max_x, max_y, min_w, min_h, xpos, ypos, scale_x, scale_y);
    }
  }

  
  tft.setSwapBytes(swapBytes);
  // calculate how long it took to draw the image
  drawTime = millis() - drawTime;
  Serial.print  ("Total render time was    : "); Serial.print(drawTime); Serial.println(" ms");
  Serial.println("=====================================");
}

void handleImageSizeFitScreen(uint16_t *pImg, uint32_t win_w, uint32_t win_h, uint16_t mcu_w, uint16_t mcu_h, uint32_t max_x, uint32_t max_y, uint32_t min_w, uint32_t min_h, uint32_t xpos, uint32_t ypos) {
  while (JpegDec.read()) {
    pImg = JpegDec.pImage;
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    if (win_w != mcu_w) {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++) {
        p += mcu_w;
        for (int w = 0; w < win_w; w++) {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    uint32_t mcu_pixels = win_w * win_h;
    if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    else if ((mcu_y + win_h) >= tft.height())
      JpegDec.abort();
  }
}


// function used to decode images with bipolar algorithm
// applied to images whose scale is 2 to the power of n or integers
void handleImageSizeWithBipolarAlgo(uint16_t *pImg, uint32_t win_w, uint32_t win_h, uint16_t mcu_w, uint16_t mcu_h, uint32_t max_x, uint32_t max_y, uint32_t min_w, uint32_t min_h, uint32_t xpos, uint32_t ypos, float scale_x, float scale_y){
  while (JpegDec.read()) {
        pImg = JpegDec.pImage;
        int mcu_x = JpegDec.MCUx * mcu_w + xpos;
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;
        if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
        else win_w = min_w;
        if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
        else win_h = min_h;
        uint32_t scaled_win_w = win_w * scale_x;
        uint32_t scaled_win_h = win_h * scale_y;
        int scaled_mcu_x = mcu_x * scale_x;
        int scaled_mcu_y = mcu_y * scale_y;
        for (int y = 0; y < scaled_win_h; y++) {
            for (int x = 0; x < scaled_win_w; x++) {
                float src_x = float(x) * win_w / scaled_win_w;
                float src_y = float(y) * win_h / scaled_win_h;
                int src_x1 = int(src_x);
                int src_y1 = int(src_y);
                //int src_x2 = src_x1 + 1;
                //int src_y2 = src_y1 + 1;
                int src_x2 = src_x1 ;
                int src_y2 = src_y1 ;
                float weight_x = src_x - src_x1;
                float weight_y = src_y - src_y1;

                uint16_t p00 = *(pImg + src_x1 + src_y1 * mcu_w);
                uint16_t p10 = *(pImg + src_x2 + src_y1 * mcu_w);
                uint16_t p01 = *(pImg + src_x1 + src_y2 * mcu_w);
                uint16_t p11 = *(pImg + src_x2 + src_y2 * mcu_w);
                
                //calculate interpolated color of pixels acccording to bilinear Interpolation
                float top = p00 * (1 - weight_x) + p10 * weight_x;  //weight_x = x_ratio
                float bottom = p01 * (1 - weight_x) + p11 * weight_x; //weight_x = x_ratio
                uint16_t interpolated_color = top * (1 - weight_y) + bottom * weight_y; //weight_y = y_ratio

                // uint16_t interpolated_color = bilinearInterpolation(p00, p10, p01, p11, weight_x, weight_y);

                tft.drawPixel(scaled_mcu_x + x, scaled_mcu_y + y, interpolated_color);
            }
        }
    }
}

//function to calculate bicubic interpolation for a 4x4 neighborhood of pixels
uint16_t bicubicInterpolation(uint16_t p00, uint16_t p10, uint16_t p20, uint16_t p30,
                              uint16_t p01, uint16_t p11, uint16_t p21, uint16_t p31,
                              uint16_t p02, uint16_t p12, uint16_t p22, uint16_t p32,
                              uint16_t p03, uint16_t p13, uint16_t p23, uint16_t p33,
                              float x_ratio, float y_ratio) {
    float x_ratio2 = x_ratio * x_ratio;
    float x_ratio3 = x_ratio2 * x_ratio;
    float y_ratio2 = y_ratio * y_ratio;
    float y_ratio3 = y_ratio2 * y_ratio;

    float a00 = p11;
    float a01 = -0.5 * p10 + 0.5 * p12;
    float a02 = p10 - 2.5 * p11 + 2 * p12 - 0.5 * p13;
    float a03 = -0.5 * p10 + 1.5 * p11 - 1.5 * p12 + 0.5 * p13;

    float a10 = -0.5 * p01 + 0.5 * p21;
    float a11 = 0.25 * p00 - 0.25 * p02 - 0.25 * p20 + 0.25 * p22;
    float a12 = -0.5 * p00 + 1.25 * p01 - p02 + 0.25 * p03 + 0.5 * p20 - 1.25 * p21 + p22 - 0.25 * p23;
    float a13 = 0.25 * p00 - 0.75 * p01 + 0.75 * p02 - 0.25 * p03 - 0.25 * p20 + 0.75 * p21 - 0.75 * p22 + 0.25 * p23;

    float a20 = p01 - 2.5 * p11 + 2 * p21 - 0.5 * p31;
    float a21 = -0.5 * p00 + 0.5 * p02 + 1.25 * p10 - 1.25 * p12 - p20 + p22 - 0.25 * p30 + 0.25 * p32;
    float a22 = p00 - 2.5 * p01 + 2 * p02 - 0.5 * p03 - 2.5 * p10 + 6.25 * p11 - 5 * p12 + 1.25 * p13 + 2 * p20 - 5 * p21 + 4 * p22 - p23 - 0.5 * p30 + 1.25 * p31 - p32 + 0.25 * p33;
    float a23 = -0.5 * p00 + 1.5 * p01 - 1.5 * p02 + 0.5 * p03 + 1.25 * p10 - 3.75 * p11 + 3.75 * p12 - 1.25 * p13 - p20 + 3 * p21 - 3 * p22 + p23 + 0.25 * p30 - 0.75 * p31 + 0.75 * p32 - 0.25 * p33;

    float a30 = -0.5 * p01 + 1.5 * p11 - 1.5 * p21 + 0.5 * p31;
    float a31 = 0.25 * p00 - 0.25 * p02 - 0.75 * p10 + 0.75 * p12 + 0.75 * p20 - 0.75 * p22 + 0.25 * p30 - 0.25 * p32;
    float a32 = -0.5 * p00 + 1.25 * p01 - p02 + 0.25 * p03 + 1.5 * p10 - 3.75 * p11 + 3 * p12 - 0.75 * p13 - 1.5 * p20 + 3.75 * p21 - 3 * p22 + 0.75 * p23 + 0.5 * p30 - 1.25 * p31 + p32 - 0.25 * p33;
    float a33 = 0.25 * p00 - 0.75 * p01 + 0.75 * p02 - 0.25 * p03 - 0.75 * p10 + 2.25 * p11 - 2.25 * p12 + 0.75 * p13 + 0.75 * p20 - 2.25 * p21 + 2.25 * p22 - 0.75 * p23 - 0.25 * p30 + 0.75 * p31 - 0.75 * p32 + 0.25 * p33;

    float interpolated_color = a00 + a01 * x_ratio + a02 * x_ratio2 + a03 * x_ratio3 +
                               a10 * y_ratio + a11 * x_ratio * y_ratio + a12 * x_ratio2 * y_ratio + a13 * x_ratio3 * y_ratio +
                               a20 * y_ratio2 + a21 * x_ratio * y_ratio2 + a22 * x_ratio2 * y_ratio2 + a23 * x_ratio3 * y_ratio2 +
                               a30 * y_ratio3 + a31 * x_ratio * y_ratio3 + a32 * x_ratio2 * y_ratio3 + a33 * x_ratio3 * y_ratio3;

    return (uint16_t)interpolated_color;
}
// function used to decode images with Bicubic Algorithm
// applied to images whose scale integers
void handleImageSizeWithBicubicAlgo(uint16_t *pImg, uint32_t win_w, uint32_t win_h, uint16_t mcu_w, uint16_t mcu_h, uint32_t max_x, uint32_t max_y, uint32_t min_w, uint32_t min_h, uint32_t xpos, uint32_t ypos, float scale_x, float scale_y){
  while (JpegDec.read()) {
        pImg = JpegDec.pImage;
        int mcu_x = JpegDec.MCUx * mcu_w + xpos;
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;
        if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
        else win_w = min_w;
        if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
        else win_h = min_h;
        int scaled_mcu_x = mcu_x * scale_x;
        int scaled_mcu_y = mcu_y * scale_y;
        
        for (int y = 0; y < win_h; y++) {
          for (int x = 0; x < win_w; x++) {
              float src_x = float(x) / scale_x;
              float src_y = float(y) / scale_y;
              int src_x_int = int(src_x);
              int src_y_int = int(src_y);
              float weight_x = src_x - src_x_int;
              float weight_y = src_y - src_y_int;

              // Calculate bicubic interpolation for a 4x4 neighborhood of pixels
              uint16_t interpolated_color = bicubicInterpolation(
                  *(pImg + src_x_int - 1 + (src_y_int - 1) * mcu_w), *(pImg + src_x_int + (src_y_int - 1) * mcu_w), *(pImg + src_x_int + 1 + (src_y_int - 1) * mcu_w), *(pImg + src_x_int + 2 + (src_y_int - 1) * mcu_w),
                  *(pImg + src_x_int - 1 + src_y_int * mcu_w), *(pImg + src_x_int + src_y_int * mcu_w), *(pImg + src_x_int + 1 + src_y_int * mcu_w), *(pImg + src_x_int + 2 + src_y_int * mcu_w),
                  *(pImg + src_x_int - 1 + (src_y_int + 1) * mcu_w), *(pImg + src_x_int + (src_y_int + 1) * mcu_w), *(pImg + src_x_int + 1 + (src_y_int + 1) * mcu_w), *(pImg + src_x_int + 2 + (src_y_int + 1) * mcu_w),
                  *(pImg + src_x_int - 1 + (src_y_int + 2) * mcu_w), *(pImg + src_x_int + (src_y_int + 2) * mcu_w), *(pImg + src_x_int + 1 + (src_y_int + 2) * mcu_w), *(pImg + src_x_int + 2 + (src_y_int + 2) * mcu_w),
                  weight_x, weight_y);

              tft.drawPixel(scaled_mcu_x + x, scaled_mcu_y + y, interpolated_color);
          }
      }
    }
}


uint16_t lanczosInterpolation(uint16_t *pImg, int mcu_w, int mcu_h, int src_x, int src_y, float a) {
    float sum_r = 0, sum_g = 0, sum_b = 0;
    float sum_weight = 0;

    for (int j = src_y - int(a) + 1; j <= src_y + int(a); j++) {
        for (int i = src_x - int(a) + 1; i <= src_x + int(a); i++) {
            if (i >= 0 && i < mcu_w && j >= 0 && j < mcu_h) {
                float x_diff = fabs(float(i) - float(src_x));
                float y_diff = fabs(float(j) - float(src_y));
                float x_weight = sinc(x_diff) * sinc(x_diff / a);
                float y_weight = sinc(y_diff) * sinc(y_diff / a);
                float weight = x_weight * y_weight;
                sum_r += ((pImg[i + j * mcu_w] >> 11) & 0x1F) * weight;
                sum_g += ((pImg[i + j * mcu_w] >> 5) & 0x3F) * weight;
                sum_b += (pImg[i + j * mcu_w] & 0x1F) * weight;
                sum_weight += weight;
            }
        }
    }

    uint16_t r = constrain(sum_r / sum_weight, 0, 0x1F);
    uint16_t g = constrain(sum_g / sum_weight, 0, 0x3F);
    uint16_t b = constrain(sum_b / sum_weight, 0, 0x1F);

    return (r << 11) | (g << 5) | b;
}

float sinc(float x) {
    if (x == 0) return 1.0;
    return sin(PI * x) / (PI * x);
}
// function used to decode images with Lanczos Algorithm
// applied to images whose random scale 
void handleImageSizeWithLanczos(uint16_t *pImg, uint32_t win_w, uint32_t win_h, uint16_t mcu_w, uint16_t mcu_h, uint32_t max_x, uint32_t max_y, uint32_t min_w, uint32_t min_h, uint32_t xpos, uint32_t ypos, float scale_x, float scale_y){
  float a = 3.0;
  while (JpegDec.read()) {
        pImg = JpegDec.pImage;
        int mcu_x = JpegDec.MCUx * mcu_w + xpos;
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;
        if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
        else win_w = min_w;
        if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
        else win_h = min_h;
        int scaled_mcu_x = mcu_x * scale_x;
        int scaled_mcu_y = mcu_y * scale_y;
        
        for (int y = 0; y < win_h; y++) {
            for (int x = 0; x < win_w; x++) {
                float src_x = float(x) / scale_x;
                float src_y = float(y) / scale_y;
                int src_x_int = int(src_x);
                int src_y_int = int(src_y);

                uint16_t interpolated_color = lanczosInterpolation(pImg, mcu_w, mcu_h, src_x_int, src_y_int, a);

                tft.drawPixel(scaled_mcu_x + x, scaled_mcu_y + y, interpolated_color);
            }
        }
    }
}

//####################################################################################################
// Print image information to the serial port (optional)
//####################################################################################################
// JpegDec.decodeFile(...) or JpegDec.decodeArray(...) must be called before this info is available!
void jpegInfo() {

  // Print information extracted from the JPEG file
  Serial.println("JPEG image info");
  Serial.println("===============");
  Serial.print("Width      :");
  Serial.println(JpegDec.width);
  Serial.print("Height     :");
  Serial.println(JpegDec.height);
  Serial.print("Components :");
  Serial.println(JpegDec.comps);
  Serial.print("MCU / row  :");
  Serial.println(JpegDec.MCUSPerRow);
  Serial.print("MCU / col  :");
  Serial.println(JpegDec.MCUSPerCol);
  Serial.print("Scan type  :");
  Serial.println(JpegDec.scanType);
  Serial.print("MCU width  :");
  Serial.println(JpegDec.MCUWidth);
  Serial.print("MCU height :");
  Serial.println(JpegDec.MCUHeight);
  Serial.println("===============");
  Serial.println("");
}

//####################################################################################################
// Show the execution time (optional)
//####################################################################################################
// WARNING: for UNO/AVR legacy reasons printing text to the screen with the Mega might not work for
// sketch sizes greater than ~70KBytes because 16 bit address pointers are used in some libraries.

