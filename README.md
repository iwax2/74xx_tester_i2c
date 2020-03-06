# 74xx_tester_i2c

電気系の実験で用いる74LSxxシリーズ、学校の実験で壊れているかどうかチェックするのが大変なので、簡単にテストできるテス
タを作りました。
Arduinoシールド型（ちょっとでかい）で、0.5秒ぐらいでICを1つテストできます。
これはI2CでI/Oエキスパンダ(MCP23017-E/SP)を接続するバージョンです。[SPIの(MCP23S17-E/SP)の派生版](https://github.com/iwax2/74xx_tester_spi "74xx_tester_spi")です。

## つかうもの
* Arduino uno(互換品でも可)
* MCP23017-E/SP(<http://akizukidenshi.com/catalog/g/gI-09486/>)
* 1602A-V2(<http://www.aitendo.com/product/10147>)
* 14pin以上のソケット(できればゼロプレッシャー) こんなの -> <http://www.aitendo.com/product/12374>
* タクトスイッチ2つ
* Arduinoシールド台（基板加工機を使って作る、PCB製造業者に依頼する、自分でエッチングする、Arduino用のユニバーサル基板シールドを加工する、[ユニバーサル](http://diary-kuzenikike.blogspot.jp/2010/03/arduino.html "Arduino用のユニバーサル基板をつくる")[基板Cで頑張る](http://memo.tank.jp/archives/1182 "頑丈な自作プロトシールド")　など）

## 対応IC
*  "IV1 (74LS04)", // 1-input NOT
*  "BUF (74LS07)", // 1-input Buffer
*  "AN2 (74LS08)", // 2-input AND
*  "OR2 (74LS32)", // 2-input OR
*  "ND2 (74LS00)", // 2-input NAND
*  "ND3 (74LS10)", // 3-input NAND
*  "ND4 (74LS20)", // 4-input NAND
*  "NR2 (74LS02)", // 2-input NOR
*  "EO2 (74LS86)", // 2-input XOR
*  "DFF (74LS74)", // D flip-flop
*  "JKF(74LS107)", // JK flip-flop
*  "BC4(74LS393)"  // Dual 4-Bit Binary Counter

## つかいかた
1. Arduinoにシールドを指します
2. テストしたいICをゼロプレッシャーソケットにセットします
3. セットしたICの型番をselectボタンで選択します
4. testボタンを押して、OKが出れば正常、NGが出れば壊れています

## Arduinoへのスケッチの書き込み
`tester_74xx_i2c.ino`をArduino IDE (1.8.10で確認)で書き込んでください

![加工した基板の例](/img/kiban.jpg)  

FusionPCBで製造（ちょっとTestとSelectのシルク間違えた）

