#include "core.h"

namespace rt::core
{
  int FrameObject::s_frame_id_counter = 1;
  int CownObject::s_id_counter = 1;
  int ThreadObject::thread_id_counter = 1;
  verona::interpreter::Scheduler* CownObject::global_scheduler = nullptr;
  void CownObject::set_Scheduler(verona::interpreter::Scheduler* instance)
  {
    assert(global_scheduler == nullptr);
    global_scheduler = instance;
  }
}