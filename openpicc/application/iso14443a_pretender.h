#ifndef ISO14443A_PRETENDER_H_
#define ISO14443A_PRETENDER_H_

extern void iso14443a_pretender (void *pvParameters);

extern int set_UID(u_int8_t *uid, size_t len);
extern int get_UID(u_int8_t *uid, size_t len);

extern int set_nonce(u_int8_t *uid, size_t len);
extern int get_nonce(u_int8_t *uid, size_t len);

#endif /*ISO14443A_PRETENDER_H_*/
