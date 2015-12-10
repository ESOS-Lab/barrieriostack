/*
* es705-slim.h  --  Audience eS705 slimbus interface
*
* Copyright 2011 Audience, Inc.
*
* Author: Greg Clemson <gclemson@audience.com>
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef _ES705_SLIM_H
#define _ES705_SLIM_H
void es705_init_slim_slave(struct slim_device *sbdev);
void es705_slim_map_channels(struct es705_priv *es705);
extern struct slim_driver es70x_slim_driver;
extern struct snd_soc_dai_ops es705_slim_port_dai_ops;
#endif
