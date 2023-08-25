# ESP32_Image_Reading_Algorithm
ESP32 reading images from SD card and scaling them to fit screen size using algorithm.

Too lazy to write a Readme myself, so here is a summary I generated from ChatGPT. The full document will be updated soon.

Library Imports and Definitions:

The code starts by importing necessary libraries such as SD.h, TFT_eSPI.h, JPEGDecoder.h, and SPI.h.
Some pins are defined using #define and const statements for SD card chip select, TFT screen chip select, and a button pin.
Setup Function:

The setup() function initializes serial communication, TFT screen, SD card, and other hardware settings.
It also scans the SD card for image files with supported extensions and stores their names in an array.
Loop Function:

The loop() function continuously reads the state of a button.
If the button state changes and the debounce delay has passed, the code advances to the next image in the array and displays it on the TFT screen.
Image File Scanning:

The scanImages() function scans the root directory of the SD card and collects the names of image files with supported extensions (e.g., .jpg, .jpeg, .png, .bmp).
Image Display Functions:

The code contains several functions for displaying images on the TFT screen:
drawSdJpeg(): Decodes and draws a JPEG image from the SD card to the TFT screen.
jpegRender(): Renders the image onto the screen using various algorithms based on the scale of the image.
Scaling Algorithms:

The code contains several scaling algorithms for resizing images to fit the screen or achieve a specific scale:
handleImageSizeFitScreen(): Scales the image using a fit-to-screen approach.
handleImageSizeWithBipolarAlgo(): Scales the image using a bipolar interpolation algorithm for integer scales.
handleImageSizeWithBicubicAlgo(): Scales the image using bicubic interpolation for integer scales.
handleImageSizeWithLanczos(): Scales the image using Lanczos interpolation for arbitrary scales.
Helper Functions:

There are also several helper functions for interpolation and information display:
bilinearInterpolation(): Interpolates pixel colors using bilinear interpolation.
bicubicInterpolation(): Interpolates pixel colors using bicubic interpolation.
lanczosInterpolation(): Interpolates pixel colors using Lanczos interpolation.
sinc(): Calculates the sinc function used in some interpolation algorithms.
jpegInfo(): Prints information about the JPEG image to the serial monitor.
Serial Output:

Throughout the code, there are statements to output information to the serial monitor using Serial.println().
This code appears to be quite complex and involves advanced image-processing techniques for scaling and interpolation. It's designed to handle different types of scaling algorithms to display images on the TFT screen at various scales while maintaining image quality. Keep in mind that this is a high-level overview, and understanding the details of each algorithm and function might require a deeper dive into the specific algorithms used.
