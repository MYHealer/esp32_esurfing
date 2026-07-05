idf_component_register(
    SRCS
        "src/DialerClient.c"
        "src/NetClient.c"
        "src/States.c"
        "src/cipher/CipherFactory.c"
        "src/cipher/CipherUtils.c"
        "src/cipher/KeyData.c"
        "src/cipher/impl/aes_cbc.c"
        "src/cipher/impl/aes_cbc_pc.c"
        "src/cipher/impl/aes_ecb.c"
        "src/cipher/impl/aes_ecb_pc.c"
        "src/cipher/impl/des_ecb_six_pc.c"
        "src/cipher/impl/desede_cbc.c"
        "src/cipher/impl/desede_cbc_pc.c"
        "src/cipher/impl/desede_ecb.c"
        "src/cipher/impl/mod_xtea.c"
        "src/cipher/impl/mod_xtea_cbc_triple_pc.c"
        "src/cipher/impl/mod_xtea_iv.c"
        "src/cipher/impl/mod_xtea_pc.c"
        "src/cipher/impl/sm4_cbc.c"
        "src/cipher/impl/sm4_ecb.c"
        "src/cipher/impl/zuc.c"
        "src/utils/PlatformUtils.c"
        "src/utils/SimThread.c"
        "src/utils/Shutdown.c"
        "src/utils/Logger.c"
        "src/utils/cJSON.c"
    INCLUDE_DIRS
        "inc"
        "inc/cipher"
        "inc/utils"
        "src"
    REQUIRES
        esp_http_client
        esp_timer
        esp_rom
        freertos
        lwip
        mbedtls
        nvs_flash
        log
)

# mbedTLS 4.x hides private API functions behind this macro
target_compile_definitions(${COMPONENT_LIB} PUBLIC MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS)
# Suppress trigraph warning in logger fallback string
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-trigraphs -Wno-error=format-truncation)
