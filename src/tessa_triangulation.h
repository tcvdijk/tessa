#ifndef INCLUDED_TESSA_TRIANGULATION
#define INCLUDED_TESSA_TRIANGULATION


//#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_vertex_base_with_id_2.h>
#include <CGAL/Triangulation_conformer_2.h>
#include <CGAL/lloyd_optimize_mesh_2.h>
#include <CGAL/Delaunay_mesher_2.h>
#include <CGAL/Delaunay_mesh_face_base_2.h>
#include <CGAL/Delaunay_mesh_size_criteria_2.h>
#include <CGAL/Polygon_2_algorithms.h>
#include <CGAL/intersections.h>

typedef CGAL::Exact_predicates_exact_constructions_kernel K;

// === Our vertex ===

template < typename GT,
           typename Vb = CGAL::Triangulation_vertex_base_with_id_2<GT> >
class Tessa_vertex : public Vb {
public:
    typedef typename Vb::Face_handle                   Face_handle;
    typedef typename Vb::Point                         Point;
    template < typename TDS2 >
    struct Rebind_TDS {
        typedef typename Vb::template Rebind_TDS<TDS2>::Other          Vb2;
        typedef Tessa_vertex<GT, Vb2>   Other;
    };
    Tessa_vertex() : Vb() {
        Vb::id() = -1;
    }
    Tessa_vertex(const Point & p) : Vb(p) {}
    Tessa_vertex(const Point & p, Face_handle c) : Vb(p, c) {}
    Tessa_vertex(Face_handle c) : Vb(c) {}
};

// === Convenience typedefs

typedef Tessa_vertex<K>                                     Tvb;
typedef CGAL::Delaunay_mesh_face_base_2<K>                  Tfb;
typedef CGAL::Triangulation_data_structure_2<Tvb, Tfb>      Tds;
typedef CGAL::Constrained_Delaunay_triangulation_2<K,Tds>   CDT;
typedef CGAL::Delaunay_mesh_size_criteria_2<CDT>            Criteria;
typedef CDT::Point                                          Point;
typedef CDT::Segment                                        Segment;
typedef CDT::Vertex_handle                                  Vertex_handle;

#endif //ndef INCLUDED_TESSA_TRIANGULATION