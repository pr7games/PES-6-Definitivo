#pragma once
#include <atomic>
// Match surface effects must never capture or bake the editor's private actor,
// including asynchronous model loads running on worker threads.
inline thread_local unsigned pes6_editor_preview_depth=0;
inline std::atomic<unsigned> pes6_editor_preview_active{0};
struct Pes6EditorPreviewScope {
  Pes6EditorPreviewScope(){++pes6_editor_preview_depth;}
  ~Pes6EditorPreviewScope(){--pes6_editor_preview_depth;}
  Pes6EditorPreviewScope(const Pes6EditorPreviewScope&)=delete;
};
