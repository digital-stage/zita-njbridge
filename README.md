# Zita-njbridge

Command line Jack clients to transmit full quality
multichannel audio over a local IP network, with
adaptive resampling.

Main features:

* One-to-one (UDP) or one-to-many (multicast).
* Sender and receiver(s) can each have their own
  sample rate and period size.
* Up to 64 channels, 16 or 24 bit or float samples.
* Receiver(s) can select any combination of channels.
* Low latency, optional additional buffering.
* High quality jitter-free resampling.
* Graceful handling of xruns, skipped cycles, lost
  packets and freewheeling.
* IP6 fully supported.
* Requires zita-resampler, no other dependencies.

Note that this version is meant for use on a *local*
network. It may work or not on the wider internet if
receiver(s) are configured for additional buffering,
and if you are lucky. The current code will replace
any gaps in the audio stream by silence, and does not
attempt to re-insert packets that arrive out of order.


See man zita-njbridge for more info.

-- 
FA






