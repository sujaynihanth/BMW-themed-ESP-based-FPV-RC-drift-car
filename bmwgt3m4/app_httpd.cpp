#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

#define STREAM_PART "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n"

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=123456789000000000000987654321";
static const char* _STREAM_BOUNDARY = "\r\n--123456789000000000000987654321\r\n";

httpd_handle_t stream_httpd = NULL;

// Shared volatile variables for thread-safe dynamic HUD updates
volatile int hud_mode = 0;
volatile bool hud_lights = false;

void updateHUDState(int mode, bool lights) {
    hud_mode = mode;
    hud_lights = lights;
}

// --- ACCELERATED 50/50 SPLIT WITH LIVE HUD OVERLAY ---
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>ESP32-CAM FPV VR Dual View</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body, html {
            background-color: #000000;
            overflow: hidden;
            width: 100vw;
            height: 100vh;
        }
        .wrapper {
            display: flex;
            width: 100%;
            height: 100%;
        }
        .panel {
            width: 50%;
            height: 100%;
            overflow: hidden;
            display: flex;
            justify-content: center;
            align-items: center;
            background-color: #000000;
            position: relative;
        }
        .left-side {
            border-right: 2px solid #111111;
        }
        img {
            width: 100%;
            height: 100%;
            object-fit: contain;
        }
        .hud {
            position: absolute;
            top: 15px;
            left: 15px;
            color: #00FFCC;
            font-family: monospace;
            font-size: 14px;
            background: rgba(0, 0, 0, 0.7);
            padding: 8px 12px;
            border-radius: 6px;
            border: 1px solid #00FFCC;
            z-index: 10;
            pointer-events: none;
        }
    </style>
</head>
<body>
    <div class="wrapper">
        <div class="panel left-side">
            <div class="hud" id="hudText">MODE: LOADING...</div>
            <img src="/stream">
        </div>
        
        <div class="panel">
            <div class="hud" id="hudTextRight">MODE: LOADING...</div>
            <img src="/stream">
        </div>
    </div>

    <script>
        setInterval(() => {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    let modeStr = "RACING";
                    if (data.mode === 1) modeStr = "DRIFT";
                    if (data.mode === 2) modeStr = "CRUISE CONTROL";
                    
                    let text = `MODE: ${modeStr}<br>LIGHTS: ${data.lights ? 'ON' : 'OFF'}`;
                    document.getElementById('hudText').innerHTML = text;
                    document.getElementById('hudTextRight').innerHTML = text;
                }).catch(err => {});
        }, 500);
    </script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t status_handler(httpd_req_t *req){
    char json_buffer[64];
    snprintf(json_buffer, sizeof(json_buffer), "{\"mode\":%d,\"lights\":%s}", hud_mode, hud_lights ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json_buffer, strlen(json_buffer));
}

static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf = (char *)malloc(64);

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        free(part_buf);
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            res = ESP_FAIL;
        } else {
            if(fb->format != PIXFORMAT_JPEG){
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if(!jpeg_converted){
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
        if(res == ESP_OK){
            size_t hlen = snprintf(part_buf, 64, STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
    }
    free(part_buf);
    return res;
}

void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_uri_t status_uri = {
            .uri       = "/status",
            .method    = HTTP_GET,
            .handler   = status_handler,
            .user_ctx  = NULL
        };
        httpd_uri_t stream_uri = {
            .uri       = "/stream",
            .method    = HTTP_GET,
            .handler   = stream_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(stream_httpd, &index_uri);
        httpd_register_uri_handler(stream_httpd, &status_uri);
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}
