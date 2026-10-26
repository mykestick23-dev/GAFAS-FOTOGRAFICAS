#include "esp_camera.h"
#include "WiFi.h"

// Pines del modelo AI-Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const int PIN_BOTON = 13;             
const char* ssid = "Gafas_Fotograficas"; 

WiFiServer server(80);
camera_fb_t * fb_global = NULL;       
bool modoStreaming = false;           

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOTON, INPUT_PULLUP);   

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  config.frame_size = FRAMESIZE_VGA;  
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Error al iniciar la cámara");
    return;
  }

  WiFi.softAP(ssid);
  server.begin();
  Serial.println("¡Wi-Fi de las Gafas Activo!");
}

void enviarImagenBase64(WiFiClient &client, camera_fb_t * fb) {
  static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i = 0, len = fb->len;
  uint8_t* buf = fb->buf;
  
  while (len >= 3) {
    uint8_t n0 = buf[i++]; uint8_t n1 = buf[i++]; uint8_t n2 = buf[i++];
    len -= 3;
    client.write(cb64[n0 >> 2]);
    client.write(cb64[((n0 & 3) << 4) | (n1 >> 4)]);
    client.write(cb64[((n1 & 15) << 2) | (n2 >> 6)]);
    client.write(cb64[n2 & 63]);
  }
  if (len > 0) {
    uint8_t n0 = buf[i++];
    uint8_t n1 = (len > 1) ? buf[i++] : 0;
    client.write(cb64[n0 >> 2]);
    client.write(cb64[((n0 & 3) << 4) | (n1 >> 4)]);
    client.write((len > 1) ? cb64[(n1 & 15) << 2] : '=');
    client.write('=');
  }
}

void loop() {
  if (digitalRead(PIN_BOTON) == LOW) {
    delay(50); 
    unsigned long tiempoInicio = millis();
    
    while(digitalRead(PIN_BOTON) == LOW) delay(10);
    unsigned long duracion = millis() - tiempoInicio;

    if (duracion >= 2000) {
      modoStreaming = !modoStreaming;
      Serial.printf("Streaming: %d\n", modoStreaming);
    } 
    else if (duracion > 50 && !modoStreaming) {
      if(fb_global) esp_camera_fb_return(fb_global); 
      fb_global = esp_camera_fb_get();               
      Serial.println("Foto fija tomada.");
    }
  }

  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();
    
    if (request.indexOf("/stream") != -1) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
      client.println();
      
      while (client.connected() && modoStreaming) {
        camera_fb_t * fb = esp_camera_fb_get();
        if (!fb) break;
        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", fb->len);
        client.write(fb->buf, fb->len);
        client.print("\r\n");
        esp_camera_fb_return(fb);
        delay(40); 
      }
      return;
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<!DOCTYPE HTML><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><meta http-equiv='refresh' content='4'></head><body style='text-align:center; font-family:sans-serif; background:#f4f7f6; margin:0; padding:20px;'>");
    client.println("<h2 style='color:#2c3e50;'>🕶️ Proyecto: Gafas Inteligentes</h2>");
    
    if (modoStreaming) {
      client.println("<p style='color:#e74c3c; font-weight:bold; font-size:18px;'>🔴 TRANSMITIENDO VIDEO EN VIVO</p>");
      client.println("<img src='/stream' style='width:100%; max-width:500px; border-radius:12px; border:4px solid #e74c3c;'>");
    } else {
      client.println("<p style='color:#34495e; font-size:16px;'>Modo Captura de Foto Fija</p>");
      if (fb_global) {
        client.print("<img src='data:image/jpeg;base64,");
        enviarImagenBase64(client, fb_global);
        client.println("' style='width:100%; max-width:500px; border-radius:12px; box-shadow: 0px 5px 15px rgba(0,0,0,0.15);'>");
      } else {
        client.println("<p style='color:#95a5a6; font-style:italic; margin-top:50px;'>Presiona el botón rápidamente para tomar una foto.</p>");
      }
    }
    client.println("</body></html>");
    client.stop();
  }
}
