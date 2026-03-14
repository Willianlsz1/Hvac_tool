// ============================================================
//  config.example.h — MODELO PÚBLICO
//  Este arquivo PODE ser enviado ao GitHub.
//
//  INSTRUÇÕES:
//  1. Copie este arquivo e renomeie para config.h
//  2. Preencha com suas credenciais reais
//  3. Nunca suba o config.h — ele está no .gitignore
// ============================================================
#pragma once

// Wi-Fi
#define WIFI_SSID_1   "NOME_DA_SUA_REDE"
#define WIFI_PASS_1   "SENHA_DA_SUA_REDE"
// #define WIFI_SSID_2   "REDE_BACKUP"
// #define WIFI_PASS_2   "SENHA_BACKUP"

// InfluxDB Cloud
// Obtenha em: cloud2.influxdata.com → Load Data → API Tokens
#define INFLUXDB_URL    "https://REGIAO.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN  "SEU_TOKEN_AQUI"
#define INFLUXDB_ORG    "SUA_ORGANIZACAO"
#define INFLUXDB_BUCKET "SEU_BUCKET"

// Identificação do dispositivo
#define DISPOSITIVO_ID  "PIEZOMETRO-01"
#define LOCAL_ID        "Nome do Local"
#define SETOR_ID        "Nome do Setor"
