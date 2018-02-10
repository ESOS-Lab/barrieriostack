/*
 * es705-export.h  --
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ES705_EXPORT_H
#define _ES705_EXPORT_H

#include <sound/soc.h>
#include <sound/soc-dai.h>

int es705_slim_set_channel_map(struct snd_soc_dai *dai,
			       unsigned int tx_num, unsigned int *tx_slot,
			       unsigned int rx_num, unsigned int *rx_slot);
#if defined(CONFIG_ARCH_MSM)
int es705_slim_get_channel_map(struct snd_soc_dai *dai,
			       unsigned int *tx_num, unsigned int *tx_slot,
			       unsigned int *rx_num, unsigned int *rx_slot);
#endif
int es705_slim_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params,
			 struct snd_soc_dai *dai);
int es705_remote_cfg_slim_rx(int dai_id);
int es705_remote_cfg_slim_tx(int dai_id);
int es705_remote_close_slim_rx(int dai_id);
int es705_remote_close_slim_tx(int dai_id);
int es705_remote_add_codec_controls(struct snd_soc_codec *codec);
int es705_remote_route_enable(struct snd_soc_dai *dai);
int es705_get_ap_esxxx_channels(int dai_id);
#endif /* _ES705_EXPORT_H */
