# Arena Design

Choices:

* Fixed Size: The size of arena is determined at creation, never changes.
  Allocating doesn't change the size --- when it runs out of space, it aborts.
* Expanding: The allocation checks and expands the arena. Choices:
    * Contiguous: Allocate next page: use virtual alloc to reserve a very large
      block (100T) and then use physical alloc to get a physical page, all
      consecutive in the virtual address space. Choices for deallocation:
        * Keep: Keep all chunks, deallocate at the end
        * Deallocate: deallocate when we reset/pop the arena, return pages to
          the system.
    * Noncontiguous: Allocate new chunk: It allocates new chunk of memory, for
      example doubling each time. The arena must maintain a list of chunks.
      Choices for deallocation:
        * Keep: Keep all chunks, deallocate at the end
        * Deallocate: deallocate when we reset/pop the arena

# Scratch Arena Design

Choices:

* Alternating arenas (raddebugger): thread local set of temporary (scratch)
  arenas, at least two if only one permanent arena is passed in call chain. For
  N permanent and temporary arenas passed (or in local scope), we must have N+1
  temporary arenas to rotate, because the N permanent+temporary arenas can be
  all temporaries (from the caller), so we need an extra one. When creating the
  scratch, we need to pass it a list of all permanent and scratch arenas, and
  it will return us an arena that is not used. Allocate choices:
    * `ArenaTemp GetScratch(Arena **conflicts, U64 conflict_count)` --- most
      general.
    * Passed as argument by value, deallocates automatically --- can't use
      expanding chunk-based arena, only fixed arena or expanding contiguous
      arena
* One big static block of memory, one arena from the bottom, one from the top.
  The two arenas rotate for scratch arenas. We can start with the bottom one as
  a permanent arena and top as a scratch arena, then rotate as the previous
  choice.
* One arena, create a temporary from it, pass it by value as a function
  argument, which will automatically pop it. I think this design can't expand
  the scratch arena in the callee.
