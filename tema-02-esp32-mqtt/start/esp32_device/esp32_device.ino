// Device-ul IoT din tema 01, de data aceasta pe un ESP32-C3 Super Mini.
//
// Aceeași logică și același protocol ca device-simulator din tema 01, cu trei
// taskuri FreeRTOS în loc de trei thread-uri Java:
//
//   task periodic --(coada de alarme)--> task sporadic
//        |                                     |
//        +---- telemetry ---> MQTT <--- alarm -+
//                              |
//                        commands --> command_received() --(coada de comenzi)--> task aperiodic ---> status
//
// Gata implementate (nu trebuie modificate):
//   config.h         setările: Wi-Fi, broker, STUDENT, valorile inițiale; DEVICE_ID
//                    se citește din adresa MAC a plăcii
//   connection.h/cpp Wi-Fi, ora exactă (SNTP), clientul MQTT, mqtt_publish(), epoch_ms()
//
// Așa cum este, schița se conectează la Wi-Fi și la broker și afișează pe
// Serial comenzile primite. Taskurile le scrieți voi: TODO 0 ... TODO 3.
// Exemplele de pornire sunt în laboratorul 0 din rt-labs-esp32c3; vezi
// exemple/README.md din folderul temei.
//
// Placa: ESP32C3 Dev Module, USB CDC On Boot: Enabled. Serial Monitor: 115200 baud.

#include "config.h"
#include "connection.h"

struct AlarmEvent {
    float temperature;
    float threshold;
};

struct CommandEvent {
    char text[COMMAND_MAX_LEN];
};

QueueHandle_t alarmQueue;
QueueHandle_t commandQueue;

volatile uint32_t current_period_ms = PERIOD_MS_INITIAL;
volatile float current_threshold = THRESHOLD_INITIAL;

// Temperatura cipului, în °C, de la senzorul intern al ESP32-C3.
// Este temperatura siliciului, nu a camerei: imediat după pornirea Wi-Fi-ului
// poate fi în jur de 50 °C, apoi scade în câteva minute spre 35-40 °C.
// Rezoluția este de aproximativ 1 °C. Ca să provocați alarme, puneți pragul
// cu SET_THRESHOLD cu un grad sub valoarea citită.
float read_temperature(void) {
    return temperatureRead();
}

// Apelată de clientul MQTT pentru fiecare comandă primită (vezi connection.h).
void command_received(const char* text) {
    Serial.printf("[mqtt] comandă primită: %s\n", text);
    // TODO 3: puneți comanda în coada de comenzi, fără să vă blocați
    //   (timeout 0). text este valid doar în timpul acestui apel: elementul
    //   cozii trebuie să conțină textul însuși (de exemplu un struct cu
    //   char text[COMMAND_MAX_LEN]), nu un pointer.
    CommandEvent cmd;
    strncpy(cmd.text, text, COMMAND_MAX_LEN);
    cmd.text[COMMAND_MAX_LEN - 1] = '\0';
    xQueueSend(commandQueue, &cmd, 0);
}

// TODO 1: TASK PERIODIC, cu perioada T (inițial PERIOD_MS_INITIAL).
//   Fiecare job:
//   - citește temperatura: read_temperature()
//   - publică telemetria: mqtt_publish("telemetry", "deviceId;timestamp;temperatura")
//     (timestamp = epoch_ms(), tipărit cu %lld; temperatura cu "%.2f")
//   - dacă temperatura depășește pragul (inițial THRESHOLD_INITIAL), pune
//     un eveniment în coada de alarme, fără să se blocheze (timeout 0).
//     Taskul periodic NU tratează alarma: o tratează taskul sporadic.
//   Eliberările respectă grila r_k = r_0 + kT: folosiți așteptare absolută
//   (xTaskDelayUntil), nu relativă (delay, vTaskDelay), ca să nu apară derivă.
//   Perioada și pragul se schimbă prin comenzi (taskul aperiodic), deci sunt
//   variabile globale volatile, citite la fiecare job.
void periodic_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    char payload[100];

    for (;;) {
        float temp = read_temperature();
        snprintf(payload, sizeof(payload), "%s;%lld;%.2f", DEVICE_ID, (long long)epoch_ms(), temp);
        mqtt_publish("telemetry", payload);

        if (temp > current_threshold) {
            AlarmEvent alarm = {temp, current_threshold};
            xQueueSend(alarmQueue, &alarm, 0); // Timeout 0
        }

        // Așteptare absolută
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(current_period_ms));
    }
}

// TODO 2: TASK SPORADIC, tratează alarmele de temperatură.
//   - așteaptă un eveniment din coada de alarme
//   - dacă de la ultima alarmă publicată au trecut mai puțin de
//     MIN_INTERARRIVAL_MS, respinge evenimentul (afișați un mesaj pe Serial)
//   - altfel publică: mqtt_publish("alarm", "deviceId;timestamp;temperatura;prag")
//   Pentru intervale folosiți un timp monoton, millis(), nu epoch_ms().
void sporadic_task(void *pvParameters) {
    AlarmEvent alarm;
    uint32_t lastExecutionTime = 0;
    char payload[100];

    for (;;) {
        if (xQueueReceive(alarmQueue, &alarm, portMAX_DELAY) == pdTRUE) {
            uint32_t currentTime = millis();
            if (currentTime - lastExecutionTime >= MIN_INTERARRIVAL_MS) {
                snprintf(payload, sizeof(payload), "%s;%lld;%.2f;%.2f", DEVICE_ID, (long long)epoch_ms(), alarm.temperature, alarm.threshold);
                mqtt_publish("alarm", payload);
                lastExecutionTime = currentTime;
                Serial.println("Alarmă trimisă.");
            } else {
                Serial.println("Alarmă respinsă (prea deasă).");
            }
        }
    }
}

// TODO 3: TASK APERIODIC, execută comenzile primite de la business service.
//   Comenzile ajung în command_received() (mai jos), care le pune în coada de
//   comenzi; fiecare comandă eliberează un job al taskului:
//     SET_INTERVAL <ms>       schimbă perioada T a taskului periodic
//     SET_THRESHOLD <valoare> schimbă pragul de alarmă
//     RESET                   readuce perioada și pragul la valorile inițiale
//     STATUS                  raportează perioada și pragul curente
//   După fiecare comandă publică o confirmare:
//     mqtt_publish("status", "deviceId;timestamp;OK SET_INTERVAL 1000")
//   O comandă necunoscută sau un parametru invalid nu oprește taskul: se
//   publică "ERROR <motiv>".
void aperiodic_task(void *pvParameters) {
    CommandEvent cmd;
    char payload[128];
    char result[64];

    for (;;) {
        if (xQueueReceive(commandQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            snprintf(result, sizeof(result), "OK %s", cmd.text);

            char* token = strtok(cmd.text, " ");
            if (token != NULL) {
                if (strcmp(token, "SET_INTERVAL") == 0) {
                    token = strtok(NULL, " ");
                    if (token) current_period_ms = atoi(token);
                } else if (strcmp(token, "SET_THRESHOLD") == 0) {
                    token = strtok(NULL, " ");
                    if (token) current_threshold = atof(token);
                } else if (strcmp(token, "RESET") == 0) {
                    current_period_ms = PERIOD_MS_INITIAL;
                    current_threshold = THRESHOLD_INITIAL;
                } else if (strcmp(token, "STATUS") == 0) {
                    snprintf(result, sizeof(result), "OK STATUS period=%lu threshold=%.2f", current_period_ms, current_threshold);
                } else {
                    snprintf(result, sizeof(result), "ERROR Unknown command");
                }
            }

            snprintf(payload, sizeof(payload), "%s;%lld;%s", DEVICE_ID, (long long)epoch_ms(), result);
            mqtt_publish("status", payload);
            Serial.printf("Comandă executată: %s\n", result);
        }
    }
}

void setup() {
    Serial.begin(115200);
    // Fără așteptare la scriere: cu placa la USB, dar fără Serial Monitor deschis,
    // fiecare Serial.printf() ar aștepta până la 2 s să fie citit și ar întârzia
    // taskul care scrie. Așa, textul pe care nu-l citește nimeni se pierde.
 //   Serial.setTxTimeoutMs(0);
    uint32_t t = millis();
    while (!Serial && millis() - t < 3000) delay(10);  // așteptăm Serial Monitor, cel mult 3 s
    delay(200);
    Serial.println();
    Serial.printf("=== tema 02: STUDENT %s, DEVICE_ID %s ===\n", STUDENT, DEVICE_ID);

    wifi_connect();
    time_sync();

    // TODO 0 (a): creați cele două cozi (xQueueCreate): coada de alarme și
    //   coada de comenzi. Înainte de mqtt_start(): de acolo încolo pot sosi
    //   comenzi, iar command_received() le pune în coada de comenzi.
    alarmQueue = xQueueCreate(10, sizeof(AlarmEvent));
    commandQueue = xQueueCreate(5, sizeof(CommandEvent));

    mqtt_start();

    // TODO 0 (b): porniți cele trei taskuri (xTaskCreate). După mqtt_start(),
    //   ca primele mesaje publicate să nu se piardă. Alegeți prioritățile
    //   între 2 și 19: peste loopTask (1), sub taskurile sistemului pentru
    //   evenimente și Wi-Fi (20, 23). Stiva, în octeți: cel puțin 4096.
    xTaskCreate(periodic_task, "Periodic", 4096, NULL, 5, NULL);
    xTaskCreate(sporadic_task, "Sporadic", 4096, NULL, 6, NULL);
    xTaskCreate(aperiodic_task, "Aperiodic", 4096, NULL, 4, NULL);
}

void loop() {
    // Totul se face în taskuri: loop() nu are nimic de făcut.
    delay(1000);
}
