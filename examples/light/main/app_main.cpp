/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <app_priv.h>
#include <app_reset.h>

static const char *TAG = "app_main";
uint16_t temperature_endpoint_id;
uint16_t pressure_endpoint_id;
uint16_t humidity_endpoint_id;

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    default:
        break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %d, effect: %d", type, effect_id);
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    esp_err_t err = ESP_OK;

    if (type == PRE_UPDATE) {
        /* Driver update */
        app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
        err = app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }

    return err;
}

extern "C" void app_main()
{
    esp_err_t err = ESP_OK;

    /* Initialize the ESP NVS layer */
    nvs_flash_init();

    /* Initialize driver */
    app_driver_handle_t temperature_handle = app_driver_temperature_init();
    app_driver_handle_t pressure_handle = app_driver_pressure_init();
    app_driver_handle_t humidity_handle = app_driver_humidity_init();
    app_driver_handle_t button_handle = app_driver_button_init();
    app_reset_button_register(button_handle);

    /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);

    // endpoint (temperature device type)
    temperature_sensor::config_t temperature_config;
    nullable<int16_t> temperature = DEFAULT_TEMPERATURE;
    temperature_config.temperature_measurement.measured_value = temperature;
    endpoint_t *temperature_endpoint = temperature_sensor::create(node, &temperature_config, ENDPOINT_FLAG_NONE, temperature_handle);

    // endpoint (pressure device type)
    pressure_sensor::config_t pressure_config;
    nullable<int16_t> pressure = DEFAULT_PRESSURE;
    pressure_config.pressure_measurement.measured_value = pressure;
    endpoint_t *pressure_endpoint = pressure_sensor::create(node, &pressure_config, ENDPOINT_FLAG_NONE, pressure_handle);

    // endpoint (humidity device type)
    humidity_sensor::config_t humidity_config;
    nullable<uint16_t> humidity = DEFAULT_HUMIDITY;
    humidity_config.humidity_measurement.measured_value = humidity;
    nullable<uint16_t> deviceId = DEfAULT_THING_NAME;
    humidity_config.humidity_measurement.min_measured_value = deviceId; // will be using this as the device id
    nullable<uint16_t> battery = DEFAULT_BATTERY;
    humidity_config.humidity_measurement.max_measured_value = battery; // will be usings this as the battery percentage
    endpoint_t *humidity_endpoint = humidity_sensor::create(node, &humidity_config, ENDPOINT_FLAG_NONE, humidity_handle);

    /* These node and endpoint handles can be used to create/add other endpoints and clusters. */
    if (!node || !temperature_endpoint || !pressure_endpoint || !humidity_endpoint) {
        ESP_LOGE(TAG, "Matter node creation failed");
    }

    // light_endpoint_id = endpoint::get_id(light_endpoint);
    temperature_endpoint_id = endpoint::get_id(temperature_endpoint);
    pressure_endpoint_id = endpoint::get_id(pressure_endpoint);
    humidity_endpoint_id = endpoint::get_id(humidity_endpoint);
    ESP_LOGI(TAG, "Temperature created with endpoint_id %d", temperature_endpoint_id);
    ESP_LOGI(TAG, "Pressure created with endpoint_id %d", pressure_endpoint_id);
    ESP_LOGI(TAG, "Humidity created with endpoint_id %d", humidity_endpoint_id);

    /* Add additional features to the node */
    // cluster_t *cluster = cluster::get(endpoint, ColorControl::Id);
    // cluster::color_control::feature::hue_saturation::config_t hue_saturation_config;
    // hue_saturation_config.current_hue = DEFAULT_HUE;
    // hue_saturation_config.current_saturation = DEFAULT_SATURATION;
    // cluster::color_control::feature::hue_saturation::add(cluster, &hue_saturation_config);

    /* Matter start */
    err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Matter start failed: %d", err);
    }

    /* Starting driver with default values */
    app_driver_temperature_set_defaults(temperature_endpoint_id);
    app_driver_pressure_set_defaults(pressure_endpoint_id);
    app_driver_humidity_set_defaults(humidity_endpoint_id);

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::wifi_register_commands();
    esp_matter::console::init();
#endif
}
