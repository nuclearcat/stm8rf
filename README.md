# stm8rf
PoC with STM8 attached to RF module, sending Manchester-encoded data with preamble and CRC8, received by RTL-SDR (rtl_433)

Current code is tested on STM8S003 + 433Mhz module connected to PC4
Can be received with RTL-SDR and rtl_433( https://github.com/merbanan/rtl_433 ):
rtl_433 -X 'n=name,m=OOK_MC_ZEROBIT,s=676,l=0,r=1776,invert,reflect,preamble={32}0x55555555,bits>=72'

For decrypting and encrypting add prefix(4b) to plaintext, nonce, as counter, it is not sent, it is "assumed" on both sides, 
Counter is increasing incrementally by timer, so clock-drift compensation should be added on receiver.
First 4 bytes, (encrypted non-ce 4b) is not sent over RF.

Also remember:
ECC recommendation 70-03, Frequency Bands For Non-Specific Short Range Devices in Europe, ERP +10dBm, <10% duty cycle
