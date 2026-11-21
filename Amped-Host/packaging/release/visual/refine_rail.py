"""Round the opening Miracle Mile rail by splitting its circumferential edges."""

from collections import Counter
from hashlib import sha256

import numpy as np

IDENTITY = "bdfe78d915face0727185c0d91cc4cca83e0dc7d77c3497e771a5f8548309444"


def edge_sets(triangles):
    return Counter(tuple(sorted((int(face[k]), int(face[(k + 1) % 3]))))
                   for face in triangles for k in range(3))


def area_vectors(positions, faces):
    a, b, c = positions[np.asarray(faces)].transpose(1, 0, 2)
    return np.cross(b - a, c - a)


def refine(mesh):
    if mesh.identity != IDENTITY or mesh.slots != (0, 1, 2):
        raise ValueError("The capture does not identify the verified close rail")
    vertices = mesh.vertices.astype(np.float64)
    points = vertices[:, 0, :3]
    along = vertices[:, 2, 1]
    order = np.argsort(along)
    rings = np.split(order, np.flatnonzero(np.diff(along[order]) > 0.6) + 1)
    unique = [np.unique(points[ring], axis=0) for ring in rings]
    if [len(row) for row in unique] != [8] * 10:
        raise ValueError("Rail must have ten rings of eight corners")
    centers = [row.mean(axis=0) for row in unique]
    ring_id = {int(i): index for index, ring in enumerate(rings) for i in ring}
    face_mask = np.linalg.norm(area_vectors(points, mesh.indices), axis=1) > 1e-6
    originals = mesh.indices[face_mask]
    output = list(vertices)
    middles = {}
    for a, b in edge_sets(originals):
        if ring_id[a] != ring_id[b]:
            continue
        chord = np.linalg.norm(points[a] - points[b])
        # Ignore cap diagonals and the small offset around the profile joint.
        if not 0.26 < chord < 0.29:
            continue
        center = centers[ring_id[a]]
        radial = points[[a, b]] - center
        midpoint = np.mean(vertices[[a, b]], axis=0)
        average = radial.mean(axis=0)
        midpoint[0, :3] = center + average / np.linalg.norm(average) * np.linalg.norm(radial, axis=1).mean()
        normal = midpoint[1, :3]
        length = np.linalg.norm(normal)
        if length <= 1e-6:
            raise ValueError("A rail edge has opposing normals")
        midpoint[1, :3] = normal / length
        middles[(a, b)] = len(output)
        output.append(midpoint)

    result, source_face = split_faces(originals, middles)
    output = np.asarray(output, dtype="<f4")
    details = verify(mesh, originals, middles, output, result, source_face)
    return output, result, details


def split_faces(originals, middles):
    """Split selected triangle edges without changing the source winding."""
    result = []
    source_face = []
    for triangle_index, original in enumerate(originals):
        faces = [tuple(map(int, original))]
        for k in range(3):
            edge = tuple(sorted((int(original[k]), int(original[(k + 1) % 3]))))
            if edge not in middles:
                continue
            for i, face in enumerate(faces):
                if not set(edge).issubset(face):
                    continue
                start = next(j for j in range(3) if face[j] in edge and face[(j + 1) % 3] in edge)
                a, b, c = face[start], face[(start + 1) % 3], face[(start + 2) % 3]
                middle = middles[edge]
                faces[i:i + 1] = [(a, middle, c), (middle, b, c)]
                break
        result.extend(faces)
        source_face.extend([triangle_index] * len(faces))

    return np.asarray(result, dtype="<u4"), source_face


def verify(mesh, originals, middles, output, result, source_face):
    """Verify original attributes, new silhouette, winding and split continuity."""
    n = len(mesh.vertices)
    if (output[:n].tobytes() != mesh.vertices.tobytes() or not np.isfinite(output).all()
            or result.max() >= len(output) or len(middles) < 60):
        raise ValueError("Rail refinement changed original attributes or produced invalid geometry")
    points = output[:, 0, :3].astype(np.float64)
    original_area = area_vectors(points, originals)
    new_area = area_vectors(points, result)
    if (np.linalg.norm(new_area, axis=1).min() < 1e-6
            or np.min((new_area * original_area[source_face]).sum(axis=1)) <= 0):
        raise ValueError("Rail refinement produced a degenerate or reversed face")

    edges = edge_sets(result)
    before = edge_sets(originals)
    deviations = []
    seams = {}
    for (a, b), mid in middles.items():
        if (edges[(a, b)] or edges[(a, mid)] != before[(a, b)]
                or edges[(b, mid)] != before[(a, b)]):
            raise ValueError("Rail edge was not split consistently on both sides")
        seam = tuple(sorted((tuple(points[a]), tuple(points[b]))))
        previous = seams.setdefault(seam, points[mid])
        if not np.array_equal(previous, points[mid]):
            raise ValueError("Duplicate positions diverged across an attribute seam")
        if not np.allclose(output[mid, 2], (mesh.vertices[a, 2] + mesh.vertices[b, 2]) / 2, atol=2e-6):
            raise ValueError("Unexpected UV change on a new rail vertex")
        deviations.append(float(np.linalg.norm(points[mid] - (points[a] + points[b]) / 2)))
    if not 0.025 < max(deviations) < 0.04:
        raise ValueError("Roundover changed the silhouette more than the measured section allows")
    unique_points, remap = np.unique(points, axis=0, return_inverse=True)
    welded = remap[result]
    welded_edges = edge_sets(welded)
    if set(welded_edges.values()) != {2} or len(unique_points) - len(welded_edges) + len(welded) != 2:
        raise ValueError("The refined rail is not a closed surface after welding attribute seams")
    return {
        "source_draw_sha256": mesh.identity,
        "source_vertices": n,
        "source_non_degenerate_triangles": len(originals),
        "source_indexed_triangles_including_strip_connectors": len(mesh.indices),
        "updated_vertices": len(output),
        "updated_triangles": len(result),
        "split_edges": len(middles),
        "original_vertex_attributes_byte_identical": True,
        "closed_after_welding_attribute_seams": True,
        "new_vertex_max_distance_from_old_edge": max(deviations),
        "gpu_buffer_bytes": output.nbytes + result.nbytes,
        "vertices_sha256": sha256(output.tobytes()).hexdigest(),
        "indices_sha256": sha256(result.tobytes()).hexdigest(),
    }
