#pragma once
#if !USE_ORIONDLL
size_t GetPluginsCount();
void CryptInstallNew(uint8_t *address, size_t size, uint8_t *result, size_t &resultSize);
#endif