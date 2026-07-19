idf_component_register(SRCS "web_server.c"
                       INCLUDE_DIRS "include"
                       EMBED_TXTFILES "../../frontend/index.html"
                                      "../../frontend/styles.css"
                                      "../../frontend/app.js"
                                      "../../frontend/manifest.json"
                       REQUIRES command_router device_state esp_app_format
                                esp_http_server esp_timer espressif__cjson
                                profile_store session_manager)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
