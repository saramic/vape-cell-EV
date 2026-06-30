---
layout: post
title:  "Vape Cell EV - part I - What's in a Vape"
date:   2026-06-29 12:00:00 +1000
---

> Disposable vapes are rechargeable batteries wrapped in a throwaway habit.

When vapes first hit our streets, just before the COVID lockdowns in 2019, I
thought: surely these devices have some reusable value. On various bike rides
and dog walks I picked up a bunch, thinking better in my junk box than flowing
into drains, rivers and waterways. At the time I heard of one or two people
suggesting they have lithium batteries inside — not always well marked, but
rechargeable, for all intents and purposes. I was mindful of the dangers of
recharging unknown battery types, but curious all the same.

My brain seems to be wired in a waste-not mindset. Maybe it is influenced
through growing up under a Communist regime of limited resources — the idea of
making the most of what you have through collecting, fixing, reusing and
repurposing. Refusing to bin useful hardware.

So slowly my junk box filled up with disposed vapes.

![Vapes I have collected, and some already dissassembed for their batteries](
/vape-cell-EV/assets/20260629_vape_collection.jpg)

The internals are pretty self-explanatory: a battery, a vape liquid dispenser
and a few pieces of electronics. Sometimes I even collected a number of vapes
that were made for reuse, presumably just lost by the addict, including the USB
recharging hardware.

I thought that this [EZ-EV challenge][element-14-EZ-EV] would be a perfect
opportunity for me to finally get my head around battery charging and
reconditioning. Not only vapes, but I had a bunch of other batteries that might
need some conditioning or could be put to use in some way.

![Disused and broken headphones disssassembled for their batteries](
/vape-cell-EV/assets/20260629_headphone_batteries.jpg)

_Not only vape cells but any disused e-waste like old headphones or phones_

In researching the state of vape reuse, I can see that this has taken a huge
step forward. Examples out there suggest people building power walls out of
them, as well as little ocarinas. Some are even extracting the lithium for
chemistry experiments.

On the general e-waste front, here in Melbourne there are a number of consumer
shops that let you recycle your e-waste: batteries, phones, electronics. Of
course my junk boxes are just like an e-waste recycling centre, without the
recycling so far. The local Telco has only recently released a build of a
synthesiser constructed completely from e-waste. This is inspiring stuff.

Now onto the EV. I had an old robot vacuum cleaner I could use as a base, and a
chassis for a "Braitenberg vehicle" I once started, but seeing a recent post on
a battery-powered train made me think I should rekindle another one of my
hobbies — model railways. The final platform might be an N-scale train EV,
inspired by Fortescue's all-electric heavy-haul trains running across the
Pilbara in Western Australia.

![Vape okarina, home battery, e-waste synth and infinty train](
/vape-cell-EV/assets/20260629_vape_reuse.jpg)

_Clockwise from top: NYU's vape synth as featured in WIRED, Chris Doel and his
vape power wall project, Fortescue's battery powered "infinity train" and
Telstra's e-waste synth_

## The Problem

Reuse is only credible if the cells are monitored properly, not guessed at. A
salvaged cell pulled from a discarded vape has an unknown history — it may have
been deep-discharged, overcharged, or just plain tired. Dropping it into a pack
with a simple cutoff board and hoping for the best is not good enough.

## The solution

What this project calls for is a smart BMS (Battery Management System)
approach: per-cell visibility, voltage and temperature tracking, and
health-aware decisions. Rather than treating the pack as a black box, each cell
gets its own attention — flagging weak cells before they drag down the rest,
and making charge and discharge decisions based on real data rather than
assumptions.

That intelligence matters most when working with salvaged cells, because mixed
history and uneven quality demand observability. A cell that looks fine in a
spot-check can hide a problem that only shows up under load or over time. The
monitoring stack has to catch that.

The hardware backbone for this is an `RS485` bus connecting the cell monitors —
a robust, noise-tolerant protocol well suited to the electrically messy
environment of a discharging battery pack. `RS485` is the same bus used in
industrial **BMS** stacks and **Modbus** sensors, which means the tooling and
libraries are mature.

Sitting at the top of that bus is the **UNO Q**: a board that pairs a real-time
**STM32** microcontroller with a Qualcomm processor running Linux. The
**STM32** side handles the timing-critical work — polling cells over `RS485`,
enforcing cutoff thresholds — while the Linux side opens up a much richer
analytical layer. Docker containers, **YOLO** inference models, data logging:
things that would be unthinkable on a bare microcontroller become
straightforward. There is also an appealing secondary use for the **UNO Q**:
watching the train run a lap and visually monitoring the pack discharge in real
time, with the Linux side offering enough headroom to run a live dashboard or
even a camera-based model that tracks the loco position against battery state.


**VapeCell EV** takes discarded lithium, instruments it with a smart monitoring
stack, and turns e-waste into motion.

<img
  width="100%"
  alt='Kids follow mini n-scale "shrek" train as it drives past'
  src="/vape-cell-EV/assets/20260630_shrek_train_and_kids.gif" />

## Next

- get some battery charging stats using `CC`/`CV` (Constant Current and
  Constant Voltage)
- look at tracking data with the **UNO Q**
- see how best to utilise the **UNO Q** capabilities for a "Smart BMS"

## Source

[https://github.com/saramic/vape-cell-EV][github-vape-cell-EV]

[element-14-EZ-EV]: https://community.element14.com/challenges-projects/design-challenges/ez-ev-challenge
[github-vape-cell-EV]: https://github.com/saramic/vape-cell-EV
