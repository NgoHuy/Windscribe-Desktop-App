#ifndef WS_ASSERT_H
#define WS_ASSERT_H

#include "logger.h"
#include "../version/windscribe_version.h"  // this file must be included, there are defined WINDSCRIBE_IS_BETA or WINDSCRIBE_IS_GUINEA_PIG

#if defined(WINDSCRIBE_IS_BETA) || defined(WINDSCRIBE_IS_GUINEA_PIG)
#define WS_ASSERT(b) { \
  if (!(b)) { \
      qCDebug(LOG_ASSERT) << "Assertion failed! (" << __FILE__ << ":" << __LINE__ << ")"; \
  } \
  Q_ASSERT(b); \
}
#else
#define WS_ASSERT(b)
#endif

#endif //WS_ASSERT_H
