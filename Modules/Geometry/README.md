# Geometry

The `Geometry` module provides 2D and 3D primitive types and intersection helpers for gameplay, physics, and culling code.

## 2D Primitives

- `aabb` - Lifecycle-managed 2D axis-aligned bounding box with minimum and maximum coordinates.
- `aabb` lifecycle methods - `initialize`, copy/move initialization, `destroy`, and `move`.
- `aabb` setters - `set_bounds`, `set_minimum`, `set_minimum_x`, `set_minimum_y`, `set_maximum`, `set_maximum_x`, and `set_maximum_y`.
- `aabb` getters - `get_minimum_x`, `get_minimum_y`, `get_maximum_x`, and `get_maximum_y`.
- `circle` - Lifecycle-managed 2D circle with center and radius.
- `circle` lifecycle methods - `initialize`, copy/move initialization, `destroy`, and `move`.
- `circle` setters/getters - Set or get center coordinates and radius.
- `intersect_aabb(const aabb &first, const aabb &second)` - Tests two 2D boxes for overlap.
- `intersect_circle(const circle &first, const circle &second)` - Tests two circles for overlap.

## 3D Primitives

- `sphere` - Lifecycle-managed 3D sphere with center and radius.
- `sphere` lifecycle methods - `initialize`, copy/move initialization, `destroy`, and `move`.
- `sphere` setters/getters - Set or get center coordinates and radius.
- `intersect_sphere(const sphere &first, const sphere &second)` - Tests two spheres for overlap.
- `geometry_ray` - Ray origin and direction.
- `geometry_plane` - Plane normal and distance.
- `geometry_aabb3` - 3D axis-aligned bounding box.
- `geometry_frustum` - Six-plane view frustum.
- `geometry_raycast_hit` - Raycast result distance, point, and normal.
- `geometry_sweep_hit` - Swept collision result time, point, and normal.

## 3D Queries

- `geometry_ray_intersect_plane(...)` - Intersects a ray with a plane.
- `geometry_ray_intersect_triangle(...)` - Intersects a ray with a triangle and writes hit information.
- `geometry_ray_intersect_aabb3(...)` - Intersects a ray with a 3D box.
- `geometry_sphere_intersect_aabb3(...)` - Tests a sphere against a 3D box.
- `geometry_swept_sphere_intersect_aabb3(...)` - Checks the center's motion through
  a box expanded by the sphere radius over the time interval `[0, 1]`. An
  initial overlap reports time zero. The hit point is the sphere center at
  the reported time. The expanded-box approximation can report early hits near
  edges and corners.
- `geometry_frustum_contains_point(...)` - Tests whether a point is inside a frustum.
- `geometry_frustum_intersect_aabb3(...)` - Tests a frustum against a 3D box.

## Thread Safety

`aabb`, `circle`, and `sphere` expose `enable_thread_safety`, `disable_thread_safety`, and `is_thread_safe` for optional recursive locking around mutable primitive state.

Set up or tear down thread safety while callers have exclusive access to the
shape. Concurrent moves use ordered locks and keep each shape's mutex attached
to that shape. If a move leaves the destination destroyed, destruction or
reinitialization releases its retained mutex once concurrent use has ended.
