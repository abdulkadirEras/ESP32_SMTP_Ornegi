#include <Arduino.h>
#include <WiFi.h> // ESP32 için WiFi kütüphanesi
#include <ESP_Mail_Client.h>


#define WIFI_SSID "Xiaomi_8411"
#define WIFI_PASSWORD "qwer12345?"


#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 587//465

/*Gönderecek mail hesap */
#define AUTHOR_EMAIL "mailAtmasınıİstediğinizMailAdresi"
#define AUTHOR_PASSWORD "mailHesabınınÖzellikliUygulamaŞifresi"

/*Mailin gideceği adres*/
#define RECIPIENT_EMAIL "sicaklikDegeriniAlacakMailAdresi"

SMTPSession smtp;
SMTP_Message message;
Session_Config config;

// LM35 sensörünün bağlı olduğu ESP32 ADC pinini tanımlayın
// GPIO34, ESP32'deki yaygın bir ADC1 girişidir.
#define LM35_PIN 34
#define LED_Pin 2//GPIO 2 ledi için

#define tehlikeliSicaklik 55 //hangi sıcaklıkta mail göndermesi isteniyorsa buraya girilmeli
#define ofsetSicaklik 10.0 //lm35 ile gerçek sıcaklık değerinin arasındaki sıcaklık farkı 10 derece


//global değişkenlerin tanımlanması
int hamADCDegeri; 
float voltage;
float sicaklik;

float sicaklikOrtalamasi=0;
float sicaklikToplayicisi=0;
uint16_t sayac;

uint16_t tehlikeliSicaklik_sayac=0;
bool tekSeferGonder=false;

bool toggleYap=false;


//fonksiyon prototiplerinin tanımlanması
void sicaklik_oku();
void sicaklik_ortalamasi();
void sicaklik_kontrolu();
void mail_gonder();
void mail_gonder2();
void run_LED();

//init(yani başlangıçta ayarlanacak ve başlatılacak çevre birimleri) ayarlama fonksiyonları
void Sicaklik_Ayarlamalari();
void Mail_Ayarlamalari();


void smtpCallback(SMTP_Status status);//mailin gönderildiğini haber eden fonksiyonu

void setup() 
{
  pinMode(LED_Pin,OUTPUT);
  Sicaklik_Ayarlamalari();
  Mail_Ayarlamalari();
  
}

void loop() 
{
  sicaklik_oku();
  sicaklik_ortalamasi();
  sicaklik_kontrolu();
}



void sicaklik_oku()
{
  // Analog pinden ham değeri oku (0-4095 arası bir değer)
  hamADCDegeri = analogRead(LM35_PIN);

  // Ham ADC değerini voltaja dönüştürün.
  // ESP32'nin ADC referans voltajı genellikle 3.3V'tur ve 12 bit çözünürlük için maksimum değer 4095'tir.
  // Voltaj = (Ham ADC değeri / Maksimum ADC değeri) * Referans Voltajı
  voltage = (float)(hamADCDegeri / 4095.0) * 3.3;

  // LM35 sensörünün özellikleri: Her 10mV (0.01V) 1 santigrat dereceye karşılık gelir.
  // Yani, Volt cinsinden değeri 100 ile çarparak Santigrat dereceye çevirebiliriz.
  // Sicaklik (°C) = Voltaj (V) * 100
  sicaklik = voltage * 100.0;

  /*
  Serial.print("Ham ADC Degeri: ");
  Serial.print(rawAdcValue);
  Serial.print("\tVoltaj: ");
  Serial.print(voltage, 3); // 3 ondalık basamak hassasiyetinde
  Serial.print(" V\tSicaklik: ");
  Serial.print(sicaklik, 2); // 2 ondalık basamak hassasiyetinde
  Serial.println(" °C");
  */
  delay(10); // Her 10 ms de bir oku
}

void sicaklik_ortalamasi()
{
  sicaklikToplayicisi+=sicaklik;
  sayac++;//sayac=sayac + 1; demek

  if(sayac>=200)
  {
    sicaklikOrtalamasi=sicaklikToplayicisi/sayac;

    sicaklikOrtalamasi+=ofsetSicaklik;// gerçek sıcaklıkla arasında 10 derece fark bulunuyor bu ekleniyor

    Serial.print("ortalama sicaklik:   ");
    Serial.println(sicaklikOrtalamasi);
    
    run_LED();
    
    sicaklikToplayicisi=0;
    sayac=0;
  }
}

void sicaklik_kontrolu()
{
  if(sicaklikOrtalamasi>=tehlikeliSicaklik)
  {
    tehlikeliSicaklik_sayac++;
    delay(1);//1 ms gecikme veriliyor bunlar sayılarak git gelleri engelleyecek
  }
  else
  {
    tehlikeliSicaklik_sayac=0;
    tekSeferGonder=false;
  }

  if(tehlikeliSicaklik_sayac>2000)
  {
    if(tekSeferGonder==false)
    {
      Serial.println("Mail Gonderiliyor");
      mail_gonder();
      mail_gonder2();
    }
  
    tekSeferGonder=true;
  }
}

void mail_gonder()
{
 /* mesaj başlıklarını ayarla*/
  message.sender.name = F("AKILLI SİSTEM");
  message.sender.email = AUTHOR_EMAIL;
  message.subject = F("ESP Odanın Sıcaklık Durumu");
  message.addRecipient(F("isim"), RECIPIENT_EMAIL);
    
 
  //gonderilecek mail mesajını ayarla
  String textMsg = String("Merhabalar !\n- Oda fazla sıcak\n")+ String("Sıcaklık: ") + String(sicaklikOrtalamasi)+String("°C")+"\n Lütfen kontrol ediniz.";
  message.text.content = textMsg.c_str();
  message.text.charSet = "us-ascii";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;


  /*sunucuya bağlan */
  if (!smtp.connect(&config)){
    ESP_MAIL_PRINTF("Baglanti hatasi, Durum Kodu: %d, Hata Kodu: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }

  if (!smtp.isLoggedIn()){
    Serial.println("\nhenuz giris yapilmadi.");
  }
  else{
    if (smtp.isAuthenticated())
      Serial.println("\ngiris basarili.");
    else
      Serial.println("\nbaglinti var ama henuz giris yapilmadi.");
  }

  /*maili gönder */
  if (!MailClient.sendMail(&smtp, &message))
    ESP_MAIL_PRINTF("Hata, Durum Kodu: %d, Durum Kodu: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());


}


void Sicaklik_Ayarlamalari()
{
  // Seri iletişimi başlatın (baud rate platformio.ini'deki monitor_speed ile aynı olmalı)
  Serial.begin(115200);

  // ESP32'nin ADC'si için attenuasyon (sinyal zayıflatma) ayarı.
  // Bu, ADC'nin hangi voltaj aralığını 0-4095 arasında ölçeceğini belirler.
  // ADC_11db, 0V - 3.3V (veya Vdd) aralığını ölçmek için uygundur.
  // LM35 çıkışı genelde 0V - 1.5V aralığında olduğu için bu ayar yeterlidir.
  analogSetPinAttenuation(LM35_PIN, ADC_11db);

  // ADC çözünürlüğünü ayarlayabilirsiniz, varsayılan genellikle 12 bittir (0-4095).
  // analogReadResolution(12); // Zaten varsayılan olduğu için genellikle gerekmez.

  Serial.println("LM35 Sicaklik Okuma Baslatildi...");
}

void Mail_Ayarlamalari()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Wi-Fi ye baglaniliyor");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("baglanti IP si: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  MailClient.networkReconnect(true);

 
  smtp.debug(1);


  smtp.callback(smtpCallback);

  

  config.server.host_name = SMTP_HOST;
  config.server.port = SMTP_PORT;
  config.login.email = AUTHOR_EMAIL;
  config.login.password = AUTHOR_PASSWORD;
  config.login.user_domain = "";


  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 3;
  config.time.day_light_offset = 0;

}


void smtpCallback(SMTP_Status status){
 
  Serial.println(status.info());

  if (status.success()){
 
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Mail gonderimi basarili: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Mail gonderimi Basarisiz!!!: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
   
      SMTP_Result result = smtp.sendingResult.getItem(i);

      ESP_MAIL_PRINTF("Mail No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Durumu: %s\n", result.completed ? "basarili" : "BASARISIZ");
      ESP_MAIL_PRINTF("Tarih/Saat: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      ESP_MAIL_PRINTF("Alici: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Konu: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

  
    smtp.sendingResult.clear();
  }
}

void run_LED()
{
  if(toggleYap==false)
  {
    digitalWrite(LED_Pin,1);
    toggleYap=true;
  }
  else
  {
    digitalWrite(LED_Pin,0);
    toggleYap=false;
  }

}