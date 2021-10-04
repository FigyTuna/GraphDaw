/* register_types.cpp */

#include "register_types.h"

#include "core/class_db.h"
#include "graphdaw.h"

void register_graphdaw_types() {
  ClassDB::register_class<LFO>();
  ClassDB::register_class<FMASynthStream>();
  ClassDB::register_class<InstrumentStreamPlayback>();
}

void unregister_graphdaw_types() {
}
