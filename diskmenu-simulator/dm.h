#ifndef _DISKMENU_API_H_
#define _DISKMENU_API_H_

// ============================================================================
//                        APPLICATION INFRASTRUCTURE
// ----------------------------------------------------------------------------

// defined in globals.h
void empty_event_handlers(void);

void done(void);

#if 0
// init event queue
void init_events( void ) {
}

// check the queue for pending events
// return 1 if found
u8 event_next( event_t *e ) {
    return 0;
}

// add event to tail of queue
// return 1 if success
u8 event_post( event_t *e ) {
    return 0;
}
#endif

#endif
