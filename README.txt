
38KHz = 1 pulse every 26.315us
8Mhz  = 1 instruction every 0.125us
 => 8-bit clock has period of 32us

560us = 17.5 clock cycles

Pin setup (Trippy RGB Wave):

IRin: PB3: 0b00001000

NEC Signal length: 9000 + 4500 + 560*6*16 = 67260us
 => 14 per second



