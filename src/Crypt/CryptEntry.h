#pragma once

namespace Crypt
{
void Init(bool is_login, uint8_t seed[4]);
void Encrypt(bool is_login, uint8_t *src, uint8_t *dest, int size);
void Decrypt(uint8_t *src, uint8_t *dest, int size);
size_t GetPluginsCount(); // FIXME: move this out

} // namespace Crypt
