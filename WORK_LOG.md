# Work Log

## Tue 28 Jul 2026

### Setup some new UNO Q

**NOTE:** _also noticed that the new Arduino App Lab mentions a new board
coming soon VENTUNO Q_
- Dragonwing IQ8 with NPU IQ-8275
- STM32H5F5 microcontroller
- RAM 16GB
- eMMC 64GB
- ready to run bricks:
  - ROS 2 (Robot Operating System 2) compatible
  - local LLMs like Qwen, VLM (Visual Language Model)
  - Melo TTS and Whisper
  - MediaPipe gesture recognition
  - YOLO-X object tracking
  - PoseNet for pose tracking
- check out Github for things still being developed

Add short cut ssh config to new board

```sh
# generate a key or use existing
ssh-keygen -o -a 100 -t ed25519

# with a specific name
find ~/.ssh/id_ed25519_UNO_Q*
~/.ssh/id_ed25519_UNO_Q
~/.ssh/id_ed25519_UNO_Q.pub

# copy to paste buffer
cat ~/.ssh/id_ed25519_UNO_Q.pub | pbcopy

# upload it to the UNO Q
ssh pollyanna.local
mkdir .ssh
chmod 700 .ssh
vi .ssh/authorized_keys
# paste it here
chmod 600 .ssh/authorized_keys

# helper to connect
cat << EOF >> ~/.ssh/config
Host athena
    # HostName athena.local
    HostName 192.168.68.132
    User arduino
    IdentityFile ~/.ssh/id_ed25519_UNO_Q
    IdentitiesOnly yes
EOF

# put it in shell mode
cat << EOF >> ~/.bashrc

  # VI everywhere
  set -o vi
EOF

# turn off graphical mode
sudo systemctl get-default
> graphical.target

sudo systemctl set-default multi-user.target

sudo systemctl get-default
multi-user.target

# restart
sudo shutdown 0
```

## TODO

NEXT:

- UNO Q dev env setup and update
- literature on re-using vape cells

**More on battery charging**

- [x] My Power Bank Rivals Commercial Ones?! Super Fast! (DIY or Buy) -
      GreatScott!

  [![
  My Power Bank Rivals Commercial Ones?! Super Fast! (DIY or Buy) -
  GreatScott!
](http://i.ytimg.com/vi/_WI9Nwqvplo/hqdefault.jpg)](https://youtu.be/_WI9Nwqvplo)
  - good watch
  - not really much about the power bank - recommends other videos
  - key is a USB C power bank board that he can power via his own power bank
  - he still uses a BMS to charge his powerbank, separate from the board above

- [x] The Surprising Flaws in 18650 Lithium-Ion Batteries - Adam Savage’s
      Tested

  [![
  The Surprising Flaws in 18650 Lithium-Ion Batteries - Adam Savage’s Tested
](http://i.ytimg.com/vi/-Y23nfAOiXQ/hqdefault.jpg)](https://youtu.be/-Y23nfAOiXQ)
  - Lumafield's Battery quality report:
    https://www.lumafield.com/battery-report
  - using Lumafield's CT scanner, previewing cheap batteries 18650's with badly
    aligned anodes.
  - felt more like an add for buying brand name cells
  - mention of a garage fire

- [x] Don't Fast Charge your Phone before Watching this Video! - GreatScott!

  [![
  Don't Fast Charge your Phone before Watching this Video! - GreatScott!
](http://i.ytimg.com/vi/iMn2yVoEqPs/hqdefault.jpg)](https://youtu.be/iMn2yVoEqPs)
  - have watched this before
  - main idea is the circuit to discharge a batter with a known current, so
    based on time, can calculate it's capacity
  - only discharge to 3V
  - rig to repeat to test impact on charge/discharge cycle

- [x] simple homemade BMS
  - **Homemade BMS - Balanced LiPo Charger Multiple Cells and Current Limit -
    Electronoobs**

  [![
  Homemade BMS - Balanced LiPo Charger Multiple Cells and Current Limit -
  Electronoobs
](http://i.ytimg.com/vi/qRVEJjk5B_g/hqdefault.jpg)](https://youtu.be/qRVEJjk5B_g)
  - shows need for separate charging of batteries
    - need balanced charging - get to 4.2v safely
    - using zener TL431 reference voltage using trim pot and BD140
    - fails over to using diodes as a load
    - LM317 to limit charging current 600mA
    - another LM317 to adjust voltage at 4.2V
  - https://electronoobs.io/shop/

- **BMS (Battery Management System) || DIY or Buy || Properly protecting
  Li-Ion/Li-Po Battery Packs - GreatScott!**

  [![
  BMS (Battery Management System) || DIY or Buy || Properly protecting
  Li-Ion/Li-Po Battery Packs - GreatScott!
](http://i.ytimg.com/vi/rT-1gvkFj60/hqdefault.jpg)](https://youtu.be/rT-1gvkFj60)
  - DIY or buy
    - [AliExpress: ~$4 3S 4S 40A 60A Li-ion Lithium Battery Protection Board
      BMS 12V 16.8V Overcharge Protection with Balance Enhanced for Drill
      DIY](https://www.aliexpress.com/item/1005012031202191.html)
  - https://github.com/stuartpittaway/diyBMS
  - https://github.com/chickey/diyBMS
  - pretty cool but a pretty difficult build
    - has a Web view via ESP32
    - can set voltage calibration
    - can discharge cells
    - have thermistor

- Most BMSs (Battery Management Systems) don't cut it... I Built a BETTER
  One. - Haase Industries

  [![
  Most BMSs (Battery Management Systems) don't cut it... I Built a BETTER One. - Haase Industries
](http://i.ytimg.com/vi/UUr-CJudg38/hqdefault.jpg)](https://youtu.be/UUr-CJudg38)
  - has a go at the Great Scott DIY BMS as well as simple homemade BMS by
    Electronoobs
  - although a bit hard to know what he is exactly on about appart from the
    waste of current in the alternative shunting and using some maybe
    slightly smarter components:
    - [Digikey: Infineon Technologies IQDH35N03LM5ATMA1 N-ch 30v 66A
      MOSFET](https://www.digikey.com.au/en/products/detail/infineon-technologies/IQDH35N03LM5ATMA1/21675799)
    - [Digikey: Texas Instruments BQ7791508PWR IC Batt Li-Ion
      3-5C](https://www.digikey.com/en/products/detail/texas-instruments/BQ7791508PWR/15856804)
    - [TI: bq77915 3-5S Low Power Protector Evaluation
      Module](https://www.ti.com/lit/ug/sluubu2b/sluubu2b.pdf)
    - [TI: BQ77915 ACTIVE 3-series to 5-series stackable ultra-low-power
      primary protector with autonomous cell
      balancing](https://www.ti.com/product/BQ77915)

### BMS landscape review — where this project sits

Three approaches reviewed:

|                       | Electronoobs   | GreatScott diyBMS      | Haase Industries     | This project                |
| --------------------- | -------------- | ---------------------- | -------------------- | --------------------------- |
| Per-cell voltage      | threshold only | yes (RS485)            | yes (BQ77915)        | yes (INA219)                |
| Balancing             | diode dump     | passive bleed resistor | autonomous (BQ77915) | TBD                         |
| Temperature           | no             | yes                    | yes                  | yes (NTC)                   |
| Data logging          | no             | web UI only            | no                   | CSV on eMMC                 |
| Discharge load        | diodes (waste) | passive bleed          | n/a                  | N-scale train (useful work) |
| Intelligence          | none           | web dashboard          | hardware IC          | ML layer on Linux           |
| Cell characterisation | **no**         | **no**                 | **no**               | **yes — the whole point**   |

**The gap none of them fill: cell characterisation for salvaged cells.**

Electronoobs just protects. GreatScott monitors a pack you already trust — the
passive bleed resistors make it only practical for stationary power walls.
Haase uses better ICs (BQ77915 does autonomous hardware balancing, genuinely
good) but doesn't log anything or characterise cells.

**The "extra" this project adds — a cell passport workflow:**

1. Charge CC/CV, log full CC→CV taper via INA219 → reveals actual charge
   acceptance
2. Discharge via constant-current load, log voltage curve → real capacity (mAh)
   and voltage sag under load (proxy for internal resistance)
3. Temperature profile from NTC during both → flags cells running hot
4. Repeat 3–5 cycles → capacity fade between cycles flags dying cells
5. Score each cell: capacity vs rated, thermal rise, fade rate → keep / caution
   / discard

**The UNO Q angle:** once a corpus of good and bad discharge curves exists,
train a small anomaly model on the Linux side to classify new cells
automatically. No hobbyist BMS does this.

**The train-as-discharge-load:** instead of burning energy in resistors, the
discharge IS useful work. Energy-per-lap becomes a real metric — battery dies,
train stops, you know how much was stored.

**Honest note on Haase's BQ77915:** hardware-level protection and balancing at
silicon speed is genuinely better for a final pack than software cutoffs. Not in
competition — BQ77915 handles the protection layer, UNO Q handles the
characterisation and intelligence layer above it.

- **How to keep LiPos from burning down your house (safe lipo charging) -
  Joshua Bardwell**

  [![
    How to keep LiPos from burning down your house (safe lipo charging) - Joshua Bardwell
  ](http://i.ytimg.com/vi/n3urBpFIBgY/hqdefault.jpg)](https://youtu.be/n3urBpFIBgY)
  - good overview of battery pack sizing
  - the idea of charging outside and keep it attended

- [ ] reasonable build online with lots of build tricks of a power bank
  - How to make Super 20,000 mAh Power Bank (120W) - DIY fast charge Power
    Bank - Penguin DIY

  [![
   How to make Super 20,000 mAh Power Bank (120W) - DIY fast charge Power
   Bank - Penguin DIY
  ](http://i.ytimg.com/vi/xAyOeGTdyX4/hqdefault.jpg)](https://youtu.be/xAyOeGTdyX4)

- [ ] might have some ideas
  - I built an ADVANCED Battery Bank (Open Source) - Ben Makes Everything

  [![
  I built an ADVANCED Battery Bank (Open Source) - Ben Makes Everything
](http://i.ytimg.com/vi/i2HRpcJS6Vk/hqdefault.jpg)](https://youtu.be/i2HRpcJS6Vk)

- [ ] nice build and a bunch of extra boards
  - I Built My Dream Power Bank | CNC Aluminum - Penguin DIY

  [![
  I Built My Dream Power Bank | CNC Aluminum - Penguin DIY
](http://i.ytimg.com/vi/r30Q7xbooYs/hqdefault.jpg)](https://youtu.be/r30Q7xbooYs)

**More on vape cell reuse**

- [ ] Can you reuse these batteries? - Becky Stern

  [![
  Can you reuse these batteries? - Becky Stern
](http://i.ytimg.com/vi/qiUyMLdVyfI/hqdefault.jpg)](https://youtu.be/qiUyMLdVyfI)

- [ ] inside a disposable with charging port - bigclivedotcom

  [![
  inside a disposable with charging port - bigclivedotcom
](http://i.ytimg.com/vi/hBgaqY9CG3g/hqdefault.jpg)](https://youtu.be/hBgaqY9CG3g)

- [ ] What _Really_ happens to used Electric Car Batteries? - (you might be surprised) - JerryRigEverything

  [![
  What *Really* happens to used Electric Car Batteries? - (you might be
  surprised) - JerryRigEverything
](http://i.ytimg.com/vi/s2xrarUWVRQ/hqdefault.jpg)](https://youtu.be/s2xrarUWVRQ)

- [x] I Powered My House Using 500 Disposable vapes - Chris Doel

  [![
  I Powered My House Using 500 Disposable vapes - Chris Doel
](http://i.ytimg.com/vi/dy-wFixuRVU/hqdefault.jpg)](https://youtu.be/dy-wFixuRVU)
  - massive build
  - segragate into working and not working
  - supply from vape stores that take old vapes
  - use a battery tester to get similar size batteries
  - build out parallel and series battery to 50V
  - power house via inverter
  - all batteries are fuse connected to +ve power rail
  - THERE IS NO WAY to charge this? this was a one off charge and build
  - certainly no safe way to charge, no BMS, no cutoff when/if batteries charge
    at different rates

- [ ] I turned a VAPE into a Li-Ion BATTERY CHARGER for some reason - StezStix
      Fix?

  [![
  I turned a VAPE into a Li-Ion BATTERY CHARGER for some reason - StezStix
  Fix?
](http://i.ytimg.com/vi/gSzApAJgZA8/hqdefault.jpg)](https://youtu.be/gSzApAJgZA8)

- [ ] More free street-lithium reclamation - bigclivedotcom

  [![
  More free street-lithium reclamation - bigclivedotcom
](http://i.ytimg.com/vi/PsJMj7FtroY/hqdefault.jpg)](https://youtu.be/PsJMj7FtroY)

- [ ] Extracting Free Lithium-ion Batteries From Used Vapes - LeftyMaker

  [![
  Extracting Free Lithium-ion Batteries From Used Vapes - LeftyMaker
](http://i.ytimg.com/vi/TBy1W2_3aOg/hqdefault.jpg)](https://youtu.be/TBy1W2_3aOg)

- [ ] How I recycle vape batteries - @CidDwyer
  - https://www.youtube.com/shorts/EW8fcs8YHsE

- [ ] How to reuse VAPE batteries - FixitEasy

  [![
  How to reuse VAPE batteries - FixitEasy
](http://i.ytimg.com/vi/VIqjmY_UMhk/hqdefault.jpg)](https://youtu.be/VIqjmY_UMhk)

- [ ] Don't Toss it! 3 Fun Ways to Repurpose Disposable Vapes! - The Doubtful
      Technician

  [![
  Don't Toss it! 3 Fun Ways to Repurpose Disposable Vapes! - The Doubtful Technician
](http://i.ytimg.com/vi/kKobDxM6Thc/hqdefault.jpg)](https://youtu.be/kKobDxM6Thc)

- [ ] Reuse lipo cells - RC MULTIROTOR & ELECTRONIC

  [![
  Reuse lipo cells - RC MULTIROTOR & ELECTRONIC
](http://i.ytimg.com/vi/lWCb9tKKBQw/hqdefault.jpg)](https://youtu.be/lWCb9tKKBQw)

- [https://interestingengineering.com/innovation/youtuber-builds-power-system-using-vape-cells](https://interestingengineering.com/innovation/youtuber-builds-power-system-using-vape-cells)

  Video: YouTuber turns disposable vapes into battery wall that runs his whole
  workshop Chris Doel turned vape waste into a functioning 2.52 kWh power wall
  that runs his kettle, microwave, and computer.
  - see https://www.youtube.com/watch?v=dy-wFixuRVU above ^^

- [https://www.instructables.com/How-to-Reuse-Disposable-Vape-Lithium-Batteries/](https://www.instructables.com/How-to-Reuse-Disposable-Vape-Lithium-Batteries/)

  How to Reuse Disposable Vape Lithium Batteries By bekathwia

- https://www.instagram.com/reels/DXcNKdHhr7r/

  > What sounds like a “dying robot kazoo” and keeps the lithium batteries from
  > used vapes out of the landfill? Vape Synth! The tiny novelty synthesizer
  > created by a group of @itp_nyu makers playfully calls attention to the
  > serious problem of e-waste generated by the millions of disposable vapes
  > are sold in the United States every month.<br><br>To create them, the team
  > breaks apart spent Elf Bar nicotine vaporizers and hacks them into digital
  > musical instruments. The resulting device still looks like a vape
  > cartridge, but with a small speaker nestled amid an array of lights and
  > buttons. To play it, you just have to suck in the way you would on a vape.
  >
  > Just before Earth Day, we chatted with @NYUTisch faculty David Rios, Kari
  > Love, and Shuang Cai about the open-source project, which was recently
  > featured in WIRED, among other outlets. Read the article at the link in our
  > bio.
  >
  > 📹 Video by David

- [ ] NYU professors and DIY-ers turned disposable vapes into a silly sounding
      playable synth - New York University

  [![
  NYU professors and DIY-ers turned disposable vapes into a silly sounding
  playable synth - New York University
](http://i.ytimg.com/vi/W3Gt10VuNGM/hqdefault.jpg)](https://youtu.be/W3Gt10VuNGM)

- [ ] Repurposing Disposable Vape Batteries: The Why, The How, and the Vape
      Synth - The Open Source Hardware Association

  [![
  Repurposing Disposable Vape Batteries: The Why, The How, and the Vape Synth
  - The Open Source Hardware Association
    ](
    http://i.ytimg.com/vi/QmgaqjXy8qE/hqdefault.jpg
    )](https://youtu.be/QmgaqjXy8qE)

- [https://www.instagram.com/p/Csyl_5ArD-G/?hl=en](https://www.instagram.com/p/Csyl_5ArD-G/?hl=en)
- [https://www.reddit.com/r/diyelectronics/comments/1jann89/been_repurposing_rechargable_vape_batteries_any/](https://www.reddit.com/r/diyelectronics/comments/1jann89/been_repurposing_rechargable_vape_batteries_any/)
- [https://www.facebook.com/groups/DIYBATTERY/posts/3237110399917434/](https://www.facebook.com/groups/DIYBATTERY/posts/3237110399917434/)

- [x] I Turned Disposable Vapes Into Elegant Power Banks - Inventors Den

  [![
  I Turned Disposable Vapes Into Elegant Power Banks - Inventors Den
](http://i.ytimg.com/vi/sVzkVDMlBvY/hqdefault.jpg)](https://youtu.be/sVzkVDMlBvY)
  - more about elegant and wrapping them in timber, making the timber rounded,
    filling gaps with glue and tiber dust
  - fun idea of epoxy coating the timber and in particular to make see through panel
  - polish the epoxy "windows" with car headlight buffing paste and buffer
  - NO BMS - just hacked together in parallel and hope for the best

- [ ] [https://www.instagram.com/reels/DHEJ5HXIbEV/](https://www.instagram.com/reels/DHEJ5HXIbEV/)

- [ ] [https://www.tiktok.com/@whynotbuildit/video/7547364785168436493](https://www.tiktok.com/@whynotbuildit/video/7547364785168436493)

- [ ] [https://www.rs-online.com/designspark/activist-engineering-disposable-vapes-take-to-the-skies](https://www.rs-online.com/designspark/activist-engineering-disposable-vapes-take-to-the-skies)

- How to Train YOLOX on a Custom Dataset - Roboflow
  [![
  How to Train YOLOX on a Custom Dataset - Roboflow
](http://i.ytimg.com/vi/q3RbFbaQQGw/hqdefault.jpg)](https://youtu.be/q3RbFbaQQGw)
  - data set [https://public.roboflow.com/object-detection/bccd/3/download/voc](https://public.roboflow.com/object-detection/bccd/3/download/voc)
  - notebook [Colab: Train YOLOX on a Custom Dataset - YouTube.ipynb](https://colab.research.google.com/drive/1_xkARB35307P0-BTnqMy0flmYrfoYi5T#scrollTo=igwruhYxE_a7)
  - YOLO X [https://github.com/Megvii-BaseDetection/YOLOX](https://github.com/Megvii-BaseDetection/YOLOX)
  - Whitepaper [YOLOX: Exceeding YOLO Series in 2021](https://arxiv.org/pdf/2107.08430)
  - [Blog: What is Mean Average Precision (mAP) in Object Detection?](https://blog.roboflow.com/mean-average-precision/)

  > Training a YOLOX model for train detection involves gathering a diverse
  > dataset of trains, annotating bounding boxes using tools like Roboflow
  > Universe or CVAT, and fine-tuning the model starting from pre-trained COCO
  > weights.
  >
  > 1. Dataset Preparation
  >    To achieve high-accuracy detection:
  >    - Collect Data: Gather hundreds of images of trains from different angles,
  >      distances, and lighting conditions.
  >    - Labeling: Annotate the trains and separate the data into train, val, and
  >      test directories.
  >    - Format: Convert your annotations to the standard YOLO text format or
  >      Pascal VOC format, depending on your YOLOX training script.
  > 2. Setting Up YOLOX
  >
  >    You can train and test using the official YOLOX GitHub repository or MATLAB's built-in computer vision tools.
  >    Clone Repository:bashgit clone https://github.com
  >
  >    ```bash
  >    cd YOLOX
  >    pip3 install -v -e .
  >    ```
  >
  >    Configuration: Edit an experiment config file (e.g., in
  >    exps/default/yolox_s.py) to specify your train parameters, number of
  >    classes (just 1 if only detecting trains), and image size.
  >
  > 3. Training the ModelIt is highly recommended to use the COCO-pretrained
  >    weights (such as yolox_s.pth for the smallest/fastest model, or yolox_x.pth
  >    for highest accuracy).Start the training process using the command-line
  >    interface:
  >    ```bash
  >    python -m yolox.tools.train \
  >      -n yolox-s \
  >      -d 1 \
  >      -b 64 \
  >      --fp16 \
  >      -o
  >    ```
  >    Parameter breakdown: -n specifies the model size, -d is the number of GPUs,
  >    -b is your batch size, and --fp16 enables mixed-precision training to speed
  >    things up.
  > 4. Running InferenceOnce trained, select the best model checkpoint and test it
  >    on new images or videos:
  >    ```bashp
  >    python tools/demo.py video \
  >      -n yolox-s \
  >      -c /path/to/your/best_ckpt.pth \
  >      --path /path/to/train_video.mp4 \
  >      --conf 0.25 \
  >      --nms 0.45 \
  >      --save_result
  >    ```

## Sun 5 Jul 2026

### Infinity Train — detect laps with vision model on UNO Q

The idea: use the N-scale model railway as the EV discharge load, running it
around a loop until the vape cell is flat. Count laps automatically using a
camera + object detection model running on the UNO Q. "Infinity train" —
battery dies, train stops, you know how much energy was discharged.

**The pipeline at a glance:**

```
Camera → captured frame → YOLO inference → train detected? →
  crossing virtual lap line? → increment counter → log energy/lap
```

---

#### Step 1 — Camera placement

- Mount a small USB or ribbon camera so the **same section of track** (ideally
  a straight) is always in frame — this becomes your virtual "lap line"
- Overhead works well for N-scale; avoids perspective distortion on the tiny
  locomotive
- Fixed mount matters: if the camera moves, your lap-line logic breaks

---

#### Step 2 — Decide: existing dataset vs roll-your-own

**Option A — Use a public dataset from Roboflow Universe**

Search [universe.roboflow.com](https://universe.roboflow.com) for "train",
"locomotive", "model train". Unlikely to find N-scale specifically but a
general train detector might work as a starting point for transfer learning.

**Option B — Capture your own (recommended for N-scale)**

N-scale locos are tiny and look nothing like full-size trains in training data.

1. Record 5–10 min of video of the train going around — vary lighting,
   speed, maybe add/remove wagons
2. Extract frames at ~1 fps: `ffmpeg -i train.mp4 -vf fps=1 frames/frame_%04d.jpg`
3. Upload frames to [app.roboflow.com](https://app.roboflow.com) → Annotate →
   draw bounding boxes → label `train`
4. Export in **YOLO v8 format** (works for both YOLOX and Ultralytics)
5. Aim for ~200–400 annotated frames; augmentation in Roboflow (flip, blur,
   brightness) multiplies it for free

---

#### Step 3 — Model choice: YOLOX or Ultralytics YOLOv8/v11?

|                      | YOLOX (Megvii)              | Ultralytics YOLOv8/v11           |
| -------------------- | --------------------------- | -------------------------------- |
| Tutorial quality     | Good (Roboflow video below) | Excellent, huge community        |
| CLI ease             | Moderate                    | Very easy (`yolo train ...`)     |
| Edge export          | ONNX, TensorRT              | ONNX, TFLite, CoreML, Hailo, etc |
| Nano model available | yolox-nano                  | yolov8n / yolo11n                |
| Active development   | Slowing                     | Very active                      |

**Recommendation: start with Ultralytics YOLOv11-nano** — simpler CLI,
better export pipeline for edge hardware, and the Roboflow YOLOX tutorial
workflow maps 1:1 to it. If you hit a wall, YOLOX is well-documented too.

- Ultralytics docs: [https://docs.ultralytics.com](https://docs.ultralytics.com)
- YOLOX GitHub: [https://github.com/Megvii-BaseDetection/YOLOX](https://github.com/Megvii-BaseDetection/YOLOX)

---

#### Step 4 — Train the model (Google Colab or local)

**Ultralytics path (recommended):**

```bash
pip install ultralytics
yolo train model=yolo11n.pt data=train_dataset.yaml epochs=50 imgsz=640
```

The Roboflow export gives you a `data.yaml` directly — point `data=` at it.

**YOLOX path (if you prefer):**

Follow the Roboflow Colab notebook already linked in this log (see entry above
↑). Uses the BCCD dataset as a template — swap in your train dataset.

- Colab: [Train YOLOX on Custom Dataset](https://colab.research.google.com/drive/1_xkARB35307P0-BTnqMy0flmYrfoYi5T#scrollTo=igwruhYxE_a7)

Both are free on Colab T4. ~15–30 min for a nano model on a small dataset.

---

#### Step 5 — Lap counting logic

Detection alone isn't laps. The simplest approach that works:

1. Define a **virtual line** as a horizontal pixel band in the frame (e.g.
   y = 200–220px) over the straight section of track
2. Each time the detected bounding box centre crosses from above → below (or
   left → right) that band, increment `lap_count`
3. Add a **cooldown** (e.g. 3 seconds) so a slow/stopped train doesn't
   double-count

```python
# pseudocode
if bbox_cy in lap_zone and not in_cooldown:
    lap_count += 1
    last_lap_time = now
```

More robust: use Ultralytics built-in tracker (`yolo track`) — it assigns a
persistent ID so you track the same object across frames without re-triggering.

---

#### Step 6 — Deploy to UNO Q

- Export model to ONNX: `yolo export model=best.pt format=onnx`
- Run inference with `onnxruntime` or the UNO Q's native SDK
- UNO Q dev env setup is still on the TODO list (see top of this log)
- If the UNO Q is too slow for real-time, drop resolution (320×320) or use
  a lower frame rate — laps take seconds, not milliseconds

---

#### Tutorials / references

- How to Train YOLOX on a Custom Dataset — Roboflow (already ↑ in this log)

  [![How to Train YOLOX on a Custom Dataset — Roboflow](http://i.ytimg.com/vi/q3RbFbaQQGw/hqdefault.jpg)](https://youtu.be/q3RbFbaQQGw)

- [ ] Ultralytics Quickstart: [https://docs.ultralytics.com/quickstart/](https://docs.ultralytics.com/quickstart/)
- [ ] Roboflow Annotate walkthrough: [https://docs.roboflow.com/annotate](https://docs.roboflow.com/annotate)
- [ ] Ultralytics export guide (ONNX / edge targets): [https://docs.ultralytics.com/modes/export/](https://docs.ultralytics.com/modes/export/)
- [ ] Ultralytics tracking (for lap counter): [https://docs.ultralytics.com/modes/track/](https://docs.ultralytics.com/modes/track/)

---

#### Immediate next actions

- [ ] Record 5–10 min of train video on the loop
- [ ] Extract frames with ffmpeg, upload to Roboflow, annotate ~300 frames
- [ ] Train yolo11n on Colab, get a working checkpoint
- [ ] Write lap-counter script with virtual line logic
- [ ] Sort out UNO Q dev env so inference can run on-device

## Tue 30 Jun 2026

Watched somde videos. Seems a bunch of people do not use a proper BMS with per
cell charging. Probably this one from Great Scott is the best

- BMS (Battery Management System) || DIY or Buy || Properly protecting
  Li-Ion/Li-Po Battery Packs - GreatScott!

  [![
  BMS (Battery Management System) || DIY or Buy || Properly protecting
  Li-Ion/Li-Po Battery Packs - GreatScott!
](http://i.ytimg.com/vi/rT-1gvkFj60/hqdefault.jpg)](https://youtu.be/rT-1gvkFj60)
  - [https://github.com/stuartpittaway/diyBMS](https://github.com/stuartpittaway/diyBMS)
  - [https://github.com/chickey/diyBMS](https://github.com/chickey/diyBMS)

## Sat 27 Jun 2026

- got N-scale minature Railroad and track,
  - will use this as the EV - electric vehichle
  - used the MOSFET and simple arduino sketch to power it on a track
  - as no P-ch cannot easily make an H-bridge
  - could probably do it with some BJT transistors - but for now can only go in 1 direction
  - the 430Hz hum from the PWM is noticeable - might try the timer switch to move it to ~32kHz and see if it works
- need to look at a vape battery for blog post 1
- also thinking of adding my upcycling background
- local telco make a music synth from recycled e-waste
  - Telstra Partners with +61 and Bear Meets Eagle On Fire to Build a
    Synthesizer from Reclaimed E-Waste - Branding in Asia

  [![
  Telstra Partners with +61 and Bear Meets Eagle On Fire to Build a Synthesizer
  from Reclaimed E-Waste - Branding in Asia
](http://i.ytimg.com/vi/mX5pt4ZuCaM/hqdefault.jpg)](https://youtu.be/mX5pt4ZuCaM)
  - https://www.telstra.com.au/exchange/why-we-built-a-synthesiser-from-reclaimed-e-waste-with-the-avala
  - https://www.tiktok.com/@telstra/video/7654095635737595154
  - https://www.instagram.com/reels/DZ4ISifB984/

- vape power wall
- vape okarina
- real world electric train
  - https://www.reddit.com/r/EngineeringPorn/comments/1ptqcy1/worlds_largest_landmobile_batteries_equipped/

- This Train Runs on Gravity (And Never Needs Refueling) - German Science Guy

  [![
  This Train Runs on Gravity (And Never Needs Refueling) - German Science Guy
](http://i.ytimg.com/vi/b_38zdEcd70/hqdefault.jpg)](https://youtu.be/b_38zdEcd70)
  - infinity train
  - recuperation uisng induction from spinning wheels
  - the route is downhill to recover energy and then travel back uptill with
    empty wagons as ore was dumped at sea
  - similar concept in recuperation
    - https://www.topgear.com/car-news/electric/all-hail-edumper-largest-ev-world
  - not much invformation from the companny on the site

  - The physics problem that killed Fortescue’s Infinity Train - The Driven

    [![
  The physics problem that killed Fortescue’s Infinity Train - The Driven
](http://i.ytimg.com/vi/hqdefault.jpg)](https://youtu.be/2mBY8oB5ri4)

- Mining giant unveils electric train in quest for zero emissions | ABC NEWS -
  ABC News (Australia)

  [![
  Mining giant unveils electric train in quest for zero emissions | ABC NEWS -
  ABC News (Australia)
](http://i.ytimg.com/vi/iEZCcgFq3lE/hqdefault.jpg)](https://youtu.be/iEZCcgFq3lE)

- https://www.facebook.com/fortescuemetalsgroupltd/videos/we-now-have-not-one-but-two-of-the-worlds-largest-land-mobile-batteries-powering/1955758248317251/

### My Upcycling philosophy

Core framing for posts:

- Main concept: upcycling (not just recycling). Take high-value lithium cells
  from disposable products and give them a second life in an EV project.
- Big-picture frame: circular economy + right to repair + pushback against
  planned obsolescence.

Core historical examples (planned obsolescence arc):

1. 1925 Phoebus cartel: lightbulb life intentionally reduced.
2. 1930s nylon stockings: durability reduced to increase repeat sales.
3. Razor/blade model: keep the user buying consumables forever.
4. Smartphone era: sealed batteries and software-driven replacement cycles.
5. Disposable vapes: the endpoint, no reusable "handle," entire product is
   waste.

Core vape call-outs:

- Disposable vapes combine addiction economics, cheap mini lithium cells, and
  stylish single-use design.
- Central irony: critical battery materials for clean transport are being burned
  through in throwaway nicotine devices.
- Project thesis line: "VapeCell EV takes that lithium back."

Personal/philosophical call-outs to keep:

- Scarcity mindset explains why "save it, fix it, it might be useful" becomes a
  lifelong pattern.
- Eastern Bloc repair culture (kombinowac) is a strength: practical ingenuity
  under constraint.
- Tension to acknowledge: resourcefulness vs accumulation; keep items with a
  realistic path to reuse.
- The challenge structure helps: deadlines and public outputs turn hoarding into
  making.

Strong opener candidate:

"The vape industry took battery technology that could help decarbonise
transport, sealed it inside addictive disposable products, and normalized
throwing it away. This project takes that material back and proves it still has
value."

Reading/watch list (short):

- Giles Slade, Made to Break (2006)
- The Light Bulb Conspiracy (2010)
  - The Light Bulb Conspiracy (2010) with hard coded English subtitles. - Carl Wong

    [![
  The Light Bulb Conspiracy (2010) with hard coded English subtitles. - Carl Wong
](http://i.ytimg.com/vi/7ZX5uGSo-tk/hqdefault.jpg)](https://youtu.be/7ZX5uGSo-tk)

  - Planned Obsolescence documentary - The Light Bulb Conspiracy (2010) RENT / BUY
    TO MORE GREAT WORK - Documentary For Better World

    [![
  Planned Obsolescence documentary - The Light Bulb Conspiracy (2010) RENT / BUY
  TO MORE GREAT WORK - Documentary For Better World
](http://i.ytimg.com/vi/wzJI8gfpu5Y/hqdefault.jpg)](https://youtu.be/wzJI8gfpu5Y)

- Mullainathan and Shafir, Scarcity (2013)

### Forum Post 1 plan (EZ EV competition)

1. Opening hook: disposable vapes are tiny batteries wrapped in a throwaway habit.
2. State the e-waste view clearly: this is not just litter, it is stranded
   lithium and missed energy value.
3. Frame your personal stance: a waste-not mindset shaped by fixing, reusing,
   and refusing to bin useful hardware.
4. Be honest about scale at home: you have collected a pile of discarded vapes
   because you see recoverable value in them.
5. Connect to wider maker culture: people are already turning vape waste into
   useful and expressive builds.
6. Example call-out 1: vape synth projects show that "trash" devices can become
   creative instruments.
7. Example call-out 2: vape power-wall builds prove these cells can aggregate
   into meaningful stored energy.
8. Example call-out 3: vape ocarina/sound projects show playful reuse can still
   drive serious e-waste awareness.
9. Pivot to your project problem: reuse is only credible if the cells are
   monitored properly, not guessed.
10. Introduce the smart BMS angle (from proposal): per-cell visibility,
    voltage/temperature tracking, and health-aware decisions instead of a simple
    cutoff board.
11. Explain why intelligence matters for salvaged cells: mixed history and
    uneven quality demand observability and safety logic.
12. Reveal the EV direction: the final platform is an N-scale train EV inspired
    by Fortescue's Australian electric trains.
13. Explain why train model format works: compact, testable, visual, and perfect
    for demonstrating cell behavior under real load.
14. Close with the project thesis: "VapeCell EV takes discarded lithium,
    instruments it with a smart monitoring stack, and turns e-waste into
    motion."

## Mon 22 Jun 2026

### Prepare for 2 weeks away

The plan is to take a limited kit to do some real testing and live the dream of
"electronics on the road", multimeter, breadboard, soldering iron and a handful
of components. Seems the most logical idea is to charge and discharte a known
battery like 18650 (preferably from a reputable source - uh oh). The idea will
be to charge the battery directly using the FINIRSI DPS-150 and it's native
CC/CV (Continuous Current and Continuous Voltage).

- I have chosen to get some FQP30N06L mosfets to do 3.3v use in future
- might use the UNO Q for data logging on it's 16GB eMMC built in storage
- I don't think I have any power resistors so I will parallel some ¼W resistors
  instead:
  - For a constant current sink you want a sense resistor of around 1 Ω
    carrying your target discharge current. In parallel, resistors divide: two
    2.2 Ω ¼W resistors in parallel give you 1.1 Ω at ½W. Four 3.9 Ω ¼W in
    parallel give 0.975 Ω at 1W. Just make sure the total wattage rating
    exceeds your expected dissipation with margin. At 500 mA discharge current
    through 1 Ω that's 0.25 W
    — two 2.2 Ω ¼W resistors in parallel handles it comfortably.
  - For the main discharge load resistor (not the sense resistor), same
    principle. At 4.2 V discharging at ~500 mA you need roughly 8 Ω carrying
    ~2W total. Eight 68 Ω ¼W resistors in parallel gives 8.5 Ω at 2W. Ugly but
    it works — just lay them flat on the breadboard.

#### Full travel parts list

**Power & measurement**

- FNIRSI DPS-150
- 65W+ USB-C GaN charger with 20V PD to get full range of DPS-150
- Multimeter
- USB-C cable for UNO Q

**Cell & safety**

- 2–3× known 18650 cells (Samsung 30Q, Molicel P26A, or similar)
  - Buy fresh from a reputable source, not eBay
- TP4056 module × 2Backup / safety reference charger, ~$1 each
- Cell holder (single 18650, with leads)

**Constant current discharge circuit**

- FQP30N06L MOSFET
- LM358 op-amp DIP-8
- 2× 2.2 Ω ¼W resistors in parallel
- Sense resistor ~1.1 Ω — 8× 68 Ω ¼W resistors in parallel
- Discharge load ~8.5 Ω
- 10 kΩ resistors × a few - Reference divider for op-amp, ADC pullup
- 100 kΩ resistor × 1 Voltage divider for cell voltage ADC reading

**Temperature sensing**

- NTC 10 kΩ thermistors × 3–4
- hook up wire
- Small piece of kapton tape or thermal pad to hold thermistor against cell
  body

**Logging / control**

- Arduino UNO Q — Linux side logs to eMMC as CSV
- Breadboard (half-size is fine for travel)
- Jumper wires

#### Plan of attack

**Days 1–2 — bench setup and cell baseline**

Get the DPS-150 running. Set 4.2 V / 500 mA CC/CV and charge one known cell
from whatever state it arrives in to full. Watch the current taper to near zero
— that transition from CC to CV is the first useful thing to observe. No code
yet, just understand what you're looking at on the display.

**Days 3–4 — build the discharge circuit**

Wire the op-amp constant current sink on the breadboard. The classic circuit:
LM358 non-inverting input gets a reference voltage (a divider from 5V sets your
target current via V_ref = I_target × R_sense), inverting input reads across
the sense resistor, output drives the MOSFET gate. Set R_sense to ~1.1 Ω and
your reference to set ~500 mA discharge current. Test it with a bench voltage
first — set DPS-150 to 3.7 V and confirm the circuit draws a steady current.
Adjust until it's stable.

**Days 5–6 — first full charge/discharge cycle with manual logging**

Charge cell to 4.2 V via DPS-150. Connect discharge circuit. Every 5 minutes
read voltage off DPS-150 display and write it down (or just watch the screen).
Run until cell hits 3.0 V cutoff — add a simple voltage comparator cutoff if
you want to be tidy, or just watch it and disconnect manually. You now have
your first real capacity data point: mAh = current × time.

**Days 7–9 — wire up UNO Q for automated logging**

Connect cell voltage (via resistor divider to keep it in 0–3.3 V range for the
STM32 ADC), thermistor (voltage divider to ADC), and optionally the sense
resistor voltage to read actual current. Write a simple Arduino sketch on the
STM32 side that samples every 30 seconds and sends CSV over serial. On the
Linux side, a 10-line Python script reads serial and appends to a CSV file on
the eMMC. You now have automated logging with temperature.

**Days 10–12 — run repeated cycles**

Let the system charge and discharge 3–4 full cycles unattended overnight. The
UNO Q logs everything. Each morning plot the discharge curves — you'll start to
see the cell's capacity and any temperature behaviour during discharge. This is
real data for your blog post.

**Days 13–14 — reflect and document**

Write up your findings. What was the actual capacity vs rated? How much did
temperature rise during discharge? Did capacity change between cycles? This
becomes Forum Post 1 of your submission: "What's actually inside a vape battery
— and what did our first experiments reveal?"

You come home with a working logging rig, real data, and the first blog post
essentially written.

### Blog setup

Blog setup, based on prior art

~~## Thu 22 Apr 2026 - from Green Brain~~

added a Jekyll Github pages blog using the commands

```sh
mise use ruby@3.2.2
gem install jekyll bundler
jekyll new docs

cd docs

# downgrade jekyll to 3.9.6
# gem "jekyll", "~> 3.9.5" # to work with github-pages
bundle add github-pages webrick

# configure the _config.yml

# run
bundle exec jekyll serve --port 8888

# open
http://127.0.0.1:8888/vape-cell-EV/
```
