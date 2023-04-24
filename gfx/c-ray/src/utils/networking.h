//
//  networking.h
//  C-ray
//
//  Created by Valtteri on 5.1.2020.
//  Copyright © 2020-2021 Valtteri Koskivuori. All rights reserved.
//

#pragma once

#if !defined(WINDOWS) && !defined(__AROS__)

#include <unistd.h>

bool chunkedSend(int socket, const char *data, size_t *progress);

ssize_t chunkedReceive(int socket, char **data, size_t *length);

#endif
