#define _XOPEN_SOURCE 500
#include "lock_state.h"

#include "agent_state.h"
#include "list/list.h"
#include "utils/cryptUtils.h"
#include "utils/memory.h"
#include "utils/oidc_error.h"

#include <string.h>
#include <syslog.h>
#include <unistd.h>

oidc_error_t unlock(list_t* loaded, const char* password) {
  static unsigned char fail_count = 0;
  syslog(LOG_AUTHPRIV | LOG_DEBUG, "Unlocking agent");
  if (agent_state.lock_state.locked == 0) {
    syslog(LOG_AUTHPRIV | LOG_DEBUG, "Agent not locked");
    oidc_errno = OIDC_ENOTLOCKED;
    return oidc_errno;
  }

  if (compareToHash(password, agent_state.lock_state.hash)) {
    agent_state.lock_state.locked = 0;
    secFreeHashed(agent_state.lock_state.hash);
    lockDecrypt(loaded, password);
    fail_count = 0;
    syslog(LOG_AUTHPRIV | LOG_DEBUG, "Agent unlocked");
    return OIDC_SUCCESS;
  }
  oidc_errno = OIDC_EPASS;
  /* delay in 0.1s increments up to 10s */
  if (fail_count < 100) {
    fail_count++;
  }
  unsigned int delay = 100000 * fail_count;
  syslog(LOG_AUTHPRIV | LOG_DEBUG, "unlock failed, delaying %0.1lf seconds",
         (double)delay / 1000000);
  usleep(delay);
  return oidc_errno;
}

oidc_error_t lock(list_t* loaded, const char* password) {
  syslog(LOG_AUTHPRIV | LOG_DEBUG, "Locking agent");
  if (agent_state.lock_state.locked) {
    syslog(LOG_AUTHPRIV | LOG_DEBUG, "Agent already locked");
    oidc_errno = OIDC_ELOCKED;
    return oidc_errno;
  }
  lock_state_setHash(&(agent_state.lock_state), hash(password));
  if (agent_state.lock_state.hash->hash != NULL) {
    agent_state.lock_state.locked = 1;
    lockEncrypt(loaded, password);
    syslog(LOG_AUTHPRIV | LOG_DEBUG, "Agent locked");
    return OIDC_SUCCESS;
  }
  return oidc_errno;
}

void lock_state_setHash(struct lock_state* l, struct hashed* h) {
  secFreeHashed(l->hash);
  l->hash = h;
}
