// std
#include <iterator>
#include <iostream>
using std::cin, std::cout, std::endl;
using std::istream, std::ifstream;
using std::ostream, std::ofstream;
using std::istream_iterator;

#include <sstream>
using std::stringstream;

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

#include <tuple>
using std::tuple;
using std::get;

#include <exception>
using std::exception;

#include <cmath>
using std::sqrt;

// Commandline argument parser
#include <CLI/CLI.hpp>

// CGAL
#include "tessa_triangulation.h"

// logging
#include "logging.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// Parse WKT
#include "parse_wkt.h"

// For hiding "unused variable" warning
template<typename... Args> inline void unused(Args&&...) {}

void set_domain_from_rings( CDT &cdt, const vector<vector<Vertex_handle>> &polygon );
void output_edge(Vertex_handle vh1, Vertex_handle vh2, string free_for, int type);
Point construct_point_in_polygon(const vector<Vertex_handle> vhs);
void insert_chain(CDT &cdt, const parser::Points&, vector<Vertex_handle>&, map<tuple<Vertex_handle,Vertex_handle>,int>&, int&, int&, int);
int edge_type( Vertex_handle a, Vertex_handle b, map<tuple<Vertex_handle,Vertex_handle>,int>& );
int find_edge_type_bruteforce(Vertex_handle a, Vertex_handle b, map<tuple<Vertex_handle, Vertex_handle>, int>&);

int main(int argc, char **argv) {

    // Commandline argument parsing
    CLI::App app("Tessa");
    app.set_version_flag( "--version", "0.0.1" );
  
    std::string in_fname;
    CLI::Option *in_fname_opt = app.add_option("-f,--file,file", in_fname, "Input file name; reads from stdin otherwise.")
                              ->check(CLI::ExistingFile);
    
    std::string out_fname;
    CLI::Option *out_fname_opt = app.add_option("-o,--output,output", out_fname, "Output file name; writes to stdout otherwise.");
    
    CLI::Option *verbose = app.add_flag("-v,--verbose");

    CLI::Option *op_cdt = app.add_flag("--cdt","Make into conforming Delaunay triangulation.");
    
    CLI::Option *op_mesh = app.add_flag("--mesh","Make into mesh.");
    double meshing_param_B{0.125};
    app.add_option("--B",meshing_param_B,"Parameter B for meshing; see CGAL documentation.",true);

    double meshing_param_S{0};
    app.add_option("--S",meshing_param_S,"Parameter S for meshing; see CGAL documentation. Zero means disabled.",true);
    
    CLI::Option *op_gabriel = app.add_flag("--gabriel","Make into conforming Gabriel graph.");

    std::string free_for;
    app.add_option("--free-for", free_for, "String to put in the 'free_for' field of output edges." );

    CLI11_PARSE(app,argc,argv);

    // Set up logging to stderr
    console = spdlog::stderr_color_mt("console");
    console->set_pattern("[%^%l%$] %v");
	if( *verbose ) console->set_level(spdlog::level::info);
	else console->set_level(spdlog::level::err);

    // Set up input stream; redirect cin from file?
    ifstream fin;
    if( in_fname_opt->count() > 0 ) {
        fin.open(in_fname);
        cin.rdbuf(fin.rdbuf());
    }

    // Set up output stream; redirect cout to file?
    ofstream fout;
    if( out_fname_opt->count() > 0 ) {
        fout.open(out_fname);
        cout.rdbuf(fout.rdbuf());
    }
    cout << std::fixed << std::setprecision(7); // we don't want scientific notation

    // Read entire input
    string wkt_string;
    cin.unsetf( std::ios::skipws ); // don't skip whitespace
    std::copy( std::istream_iterator<char>(cin), std::istream_iterator<char>(), std::back_inserter(wkt_string) );

    // Parse a single wkt polygon
    // Watch out 1: A input.polygon can consist multiple rings
    // Watch out 2: These rings are not closed implicitly; the last input vertex
    //              should be the same as the first; we don't close it.
    auto [success,input] = parser::parse_wkt_polygon(wkt_string);
    if( !success ) {
        return 2;
    }

    // Iterate over the rings of the parsed polygon:
    // Add all line segments to the CDT
    // Vertex ids are assigned consecutively from 0
    CDT cdt;
    int index = 0;
    map<tuple<Vertex_handle,Vertex_handle>,int> chain_edges; // vector of edge sets of the rings/chains
    vector<vector<Vertex_handle>> cgal_polygon; // vector of rings; index 0 is outer ring
    cgal_polygon.reserve(input.polygon.size()+input.linestrings.size());
    int num_edges_inserted = 0;
    int type = 0; // type of edges: first ring has type 0
    for( auto &ring : input.polygon ) {
        cgal_polygon.emplace_back();
        insert_chain( cdt, ring, cgal_polygon.back(), chain_edges, index, num_edges_inserted, type );
        type = 1; // further rings have type 1
    }
    // Now also add all the linestrings from the multilinestring. (Could be empty.)
    for( auto &chain : input.linestrings ) {
        cgal_polygon.emplace_back();
        insert_chain( cdt, chain, cgal_polygon.back(), chain_edges, index, num_edges_inserted, 2 );
    }
    console->info("Number of input vertices: {}", cdt.number_of_vertices() );
    console->info("Number of edges inserted: {}", num_edges_inserted );

    // Try to come up with a point *inside* each hole.
    // Currently puts it just slightly inside a corner; not guaranteed to work.
    std::list<Point> list_of_seeds;
    for( unsigned long i=1; i<input.polygon.size(); ++i ) {
        // if this polygon is a ring (and not a path), put a seed point in it
        //if( cgal_polygon.front()==cgal_polygon.back() ) {
            Point seed = construct_point_in_polygon(cgal_polygon[i]);
            list_of_seeds.push_back(seed);
        //}
    }


    // keep track of if we did something so we can give a warning
    // *and* because we give different output in that case
    bool did_something = false;
    bool should_repair_labels = false;

    // make cdt?
    if( *op_cdt ) {
        did_something = true;
        console->info("Making conforming Delauney triangulation...");
        try {
            CGAL::make_conforming_Delaunay_2(cdt);
            set_domain_from_rings( cdt, cgal_polygon );
        } catch( exception &x ) {
            console->error(x.what());
        }
        console->info("Number of vertices is now: {}", cdt.number_of_vertices() );
    }

    // make mesh?
    if( *op_mesh ) {
        did_something = true;
        should_repair_labels = true;
        console->info("Making mesh with parameters B={} and S={} ...", meshing_param_B, meshing_param_S);
        Criteria crit(meshing_param_B,meshing_param_S);
        CGAL::refine_Delaunay_mesh_2(cdt, list_of_seeds.begin(), list_of_seeds.end(), crit);
        // this function already sets the domain correctly (holes/nonholes)

        console->info("Number of vertices is now: {}", cdt.number_of_vertices() );
    }

    // make conforming Gabriel?
    if( *op_gabriel ) {
        did_something = true;
        should_repair_labels = true;
        console->info("Making conforming Gabriel graph...");
        try {
            CGAL::make_conforming_Gabriel_2(cdt);
            set_domain_from_rings( cdt, cgal_polygon );
        } catch( exception &x ) {
            console->error(x.what());
        }
        console->info("Number of vertices is now: {}", cdt.number_of_vertices() );
    }

    // Reconstruct original labels if we may have messed them up
    if( should_repair_labels ) {
        console->info("Repairing labels");
        // This is before cleaning up the ids, so any newly introduced points have id -1
        // Adjacent edges could be mesh edges, which is fine, but maybe we subdivided
        // a "boundary", "hole" or "road" edge.
        map<tuple<Vertex_handle,Vertex_handle>,int> new_chain_edges;
        for( auto ei : cdt.finite_edges() ) {
            CDT::Face& f = *(ei.first);
            int i = ei.second;
            auto vh1 = f.vertex(f.cw(i));
            auto vh2 = f.vertex(f.ccw(i));
            if (vh1->id() == -1 || vh2->id() == -1) {
                // At least one of the vertices is new; we need to check it
                int original_type = find_edge_type_bruteforce(vh1, vh2, chain_edges);
                if( original_type != -1 ) {
                    //console->info("The segment ({},{})-({},{}) is actually of type {}", vh1->point().x(), vh1->point().y(), vh2->point().x(), vh2->point().y(), original_type);
                    new_chain_edges[{vh1,vh2}] = original_type;
                }
            }
        }
        chain_edges.merge(new_chain_edges);
        console->info("Done repairing edges");
    }

    // Clean up data structure
    if( did_something ) {
        // give ids to any vertices that were introduced
        for( auto vh : cdt.finite_vertex_handles() ) {
            if( vh->id()==-1 ) vh->id() = index++;
        }
    } else {
        console->warn("Did not do anything to the input.");
    }


    // === Output

    cout << cdt.number_of_vertices() << "\n";
    int num_edges = 0; // there is no number_of_edges?
    for( auto ei : cdt.finite_edges() ) { unused(ei); ++num_edges; } // count edges "by hand" instead
    cout << num_edges << "\n";

    { // Output vertices
        int check_index = 0;
        bool warned_bad_ids = false;
        for(auto vh : cdt.finite_vertex_handles() ) {
            if( vh->id()!=check_index++ && !warned_bad_ids ) {
                console->error("Watch out! Vertex ids are not consecutive from 0.");
                warned_bad_ids = true;
            }
            cout << vh->id() << ";" << vh->point().x() << ";" << vh->point().y() << "\n";
        }
    }

    // Output edges
    if( did_something ) {
        // output triangulation edges
        for( auto ei : cdt.finite_edges() ) {
            CDT::Face& f = *(ei.first);
            int i = ei.second;
            auto other_f = f.neighbor(i);
            if( f.is_in_domain() || other_f->is_in_domain() ) {
                auto vh1 = f.vertex(f.cw(i));
                auto vh2 = f.vertex(f.ccw(i));
                int type = edge_type(vh1,vh2,chain_edges);
                output_edge( vh1, vh2, free_for, type);
            }
        }
    } else {
        // spit out original edges
        for( auto &ring : cgal_polygon ) {
            Vertex_handle vh1 = ring[0];
            for( Vertex_handle vh2 : ring ) {
                if( vh1!=vh2 ) {
                    int type = edge_type(vh1,vh2,chain_edges);
                    output_edge( vh1, vh2, free_for, type);
                }
                vh1 = vh2;
            }
        }
    }


    // And we're done.
    console->info("Done.");

}

bool point_is_in_domain( Point p, const vector<vector<Point>> &polygon ) {
    // outside the outer ring? not in the domain
    if( CGAL::bounded_side_2(polygon[0].begin(), polygon[0].end(), p, K()) != CGAL::ON_BOUNDED_SIDE ) return false;
    // are we in any of the holes? not in the domain
    for( size_t i=1; i<polygon.size(); ++i ) {
        if( CGAL::bounded_side_2(polygon[i].begin(), polygon[i].end(), p, K()) == CGAL::ON_BOUNDED_SIDE ) return false;
    }
    // otherwise we're good
    return true;
}
void set_domain_from_rings( CDT &cdt, const vector<vector<Vertex_handle>> &vhs ) {
    // assumption: faces are triangles
    vector<vector<Point>> polygon;
    polygon.reserve(vhs.size());
    for( const vector<Vertex_handle> &ring : vhs ) {
        polygon.emplace_back();
        polygon.back().reserve(ring.size());
        for( Vertex_handle vh : ring ) {
            polygon.back().emplace_back(vh->point());
        }
    }
    for( auto f : cdt.all_face_handles() ) {
        // is the centroid in the polygon outer ring, but not in any of the holes?
        Vertex_handle vh0 = f->vertex(0);
        Vertex_handle vh1 = f->vertex(1);
        Vertex_handle vh2 = f->vertex(2);
        Point centroid = vh0->point() + 0.33*(vh1->point()-vh0->point()) + 0.33*(vh2->point()-vh0->point());
        f->set_in_domain( point_is_in_domain(centroid,polygon) );
    }
}

Point construct_point_in_polygon(const vector<Vertex_handle> vhs) {
    // Take three adjacent points on the first ring
    Point a = vhs[0]->point();
    Point b = vhs[1]->point();
    Point c = vhs[2]->point();
    // Assume abc is a convex turn: then seed is probably inside
    Point seed = b + 0.0001*((a-b) + (c-b));
    // See if we're actually inside
    vector<Point> testpoly;
    testpoly.reserve(vhs.size());
    for( auto vh : vhs ) testpoly.emplace_back(vh->point());
    bool inside = CGAL::bounded_side_2(testpoly.begin(), testpoly.end(),seed, K())==CGAL::ON_BOUNDED_SIDE;
    if( inside ) {
        console->info("Seed is inside the hole on first attempt");
    } else {
        seed = b - 0.01*((a-b) + (c-b));
        inside = CGAL::bounded_side_2(testpoly.begin(), testpoly.end(),seed, K())==CGAL::ON_BOUNDED_SIDE;
        if( inside ) {
            console->info("Seed inside the hole on second attempt");
        } else {
            console->error("Seed point not in the hole :(. Result will be bad.");
        }
    }
    return seed;
}

string type_name(int type) {
    switch(type) {
    case 0: return "boundary";
    case 1: return "hole";
    case 2: return "road";
    case 3: return "mesh";
    default: return "unknown";
    }
}
void output_edge(Vertex_handle vh1, Vertex_handle vh2, string free_for, int type) {
    string bidirectional = "1";
    double distance = to_double( (vh1->point()-vh2->point()).squared_length() );
    cout << vh1->id() << ";";
    cout << vh2->id() << ";";
    cout << distance << ";";
    cout << free_for << ";";
    cout << bidirectional << ";";
    cout << type_name(type) << ";\n";
}

void insert_chain(CDT &cdt, const parser::Points& chain, vector<Vertex_handle>& cgal_polygon, map<tuple<Vertex_handle,Vertex_handle>,int> &chain_edges, int &index, int &num_edges_inserted, int type ) {
    cgal_polygon.reserve(chain.size());
    for( parser::Point p : chain ) {
        cgal_polygon.emplace_back( cdt.insert(Point(p.x, p.y)) );
        auto &v = cgal_polygon.back();
        if( v->id()==-1 ) v->id() = index++; // only assign id if this vertex is new (-1 is default value)
    }
    int n = cgal_polygon.size();
    if( n==1 ) return; // don't try to make edges if we have only a single vertex
    for( int i=0; i<n-1; ++i ) {
        if (cgal_polygon[i]->id() == cgal_polygon[i + 1]->id()) {
            //console->warn("Skipping a length-zero edge at {} {}", cgal_polygon[i]->point().x(), cgal_polygon[i]->point().y());
            continue;
        }
        cdt.insert_constraint( cgal_polygon[i], cgal_polygon[i+1] );
        console->info("Inserting edge {} - {} with type {}",cgal_polygon[i]->id(), cgal_polygon[i+1]->id(), type );
        chain_edges[{cgal_polygon[i],cgal_polygon[i+1]}] = type;
        ++num_edges_inserted;
    }
}

int edge_type( Vertex_handle a, Vertex_handle b, map<tuple<Vertex_handle,Vertex_handle>,int> &chain_edges ) {
    auto known_edge = chain_edges.find({a,b});
    if( known_edge!=chain_edges.end() ) return known_edge->second;
    // tuple might be the other way around: check that too if we didn't find yet
    known_edge = chain_edges.find({b,a});
    if( known_edge!=chain_edges.end() ) return known_edge->second;
    return 3; // we didn't add it, so it's a mesh edge. well, or a subdivided edge :(
}

int find_edge_type_bruteforce( Vertex_handle a, Vertex_handle b, map<tuple<Vertex_handle, Vertex_handle>, int> &chain_edges ) {
    Segment needle{ a->point(), b->point() };
    for (auto &hay : chain_edges) {
        Point p1 = get<0>(hay.first)->point();
        Point p2 = get<1>(hay.first)->point();
        Segment seg{ p1, p2 };

        auto result = intersection(needle, seg);
        if (result) {
            // There is an intersection, but is it a segment or a point?
            if( boost::get<Segment>(&*result) ) {
                // The intersection is a segment! Return the type of the overlapping segment.
                return hay.second;
            }
        }

    }
    // does not overlap any original edges
    return -1;
}