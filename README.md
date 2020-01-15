# stm8rf
PoC with STM8 attached to RF module, sending Manchester-encoded data with preamble and CRC8, received by RTL-SDR (rtl_433)

Current code is tested on STM8S003 + 433Mhz module connected to PC4
Can be received with RTL-SDR and rtl_433( https://github.com/merbanan/rtl_433 ):
rtl_433 -X 'n=stm8rf,m=OOK_MC_ZEROBIT,s=676,l=0,r=1776,invert,reflect,preamble={32}0x55555555,bits>=72' 

Device has 2 stages of operation.
)Powerup & announce
On powerup it will generate/derive temporary session key (KEY_SESSION), read BEACONING_PERIOD, factory-generated key (KEY_FACTORY).
If IC don't have hardware random(HW_RND), then use eeprom counter, that is incremented each powerup. (NONCE_POWERUP)
Derivation as follows:
COUNTER is sequental or HW_RND
NONCE_POWERUP = COUNTER+KEY_FACTORY
KEY_SESSION = XXTEA(NONCE_POWERUP, 5, KEY_FACTORY)
Then, timer starts for N_STAGE1 minutes, "ANNOUNCING".
Data sent as following:
RF_DATA_PLAINTEXT = COUNTER(4b) + KEY_FACTORY(4b)
RF_DATA_CIPHERTEXT = XXTEA(RF_DATA_PLAINTEXT, 2, KEY_FACTORY)
RF_DATA_CIPHERTEXT is sent with duty cycle 9.99% for first N_STAGE1 minutes

)Beaconing
NONCE_CTR+=1 on each TX
Sleep until next beacon (TODO: derive sleep time from KEY_SESSION), this will guarantee pseudorandomness and sort of more safety against predictive jamming or spoofing.
RF_DATA_PLAINTEXT = NONCE_CTR(4b) + SENSOR_DATA(8b)
RF_DATA_CIPHERTEXT = XXTEA(RF_DATA_PLAINTEXT, 12, KEY_SESSION)


For decrypting and encrypting add prefix(4b) to plaintext, nonce, as counter, it is not sent, it is "assumed" on both sides, 
Counter is increasing incrementally by timer, so clock-drift compensation should be added on receiver.
First 4 bytes, (encrypted non-ce 4b) is not sent over RF.

TODO: Add some way to send out-of-band data, on critical situations and etc.
TODO: CTR encryption mode seems more suitable

Also remember:
ECC recommendation 70-03, Frequency Bands For Non-Specific Short Range Devices in Europe, ERP +10dBm, <10% duty cycle
