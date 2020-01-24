// AGPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

struct ui_model;

extern const char *client_types[];
extern const char *client_types_cfg[];

void load_accounts();
void write_accounts(void *fp);
void ui_accounts(ui_model &m);
