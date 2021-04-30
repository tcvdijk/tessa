#ifndef INCLUDED_PARSE_WKT
#define INCLUDED_PARSE_WKT

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_expect.hpp>
#include <boost/spirit/include/qi_no_case.hpp>
#include <boost/spirit/include/support_line_pos_iterator.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/io.hpp>

#include <vector>
#include <tuple>
#include <sstream>

#include <memory>
#include "logging.h"

namespace parser {
    struct Point {
        double x, y;
    };
    using Points = std::vector<Point>;
    using Polygon = std::vector<Points>;
    using LineString = Points;
    using MultiLineString = std::vector<LineString>;
    //using TessaInput = std::tuple<Polygon,MultiLineString>;
    struct TessaInput {
        Polygon polygon;
        MultiLineString linestrings;
    };
}
BOOST_FUSION_ADAPT_STRUCT(
    parser::Point,
    (double, x),
    (double, y)
)
BOOST_FUSION_ADAPT_STRUCT(
    parser::TessaInput,
    (parser::Polygon, polygon),
    (parser::MultiLineString, linestrings)
)
namespace parser {    
    namespace qi = boost::spirit::qi;
    namespace fusion = boost::fusion;
    namespace phoenix = boost::phoenix;
    namespace ascii = boost::spirit::ascii;

    TessaInput foo(const Polygon &p) { return {p,{}}; }

    template<typename Iterator>
    struct wkt_grammar : qi::grammar<Iterator,TessaInput(),ascii::space_type> {

        wkt_grammar() : wkt_grammar::base_type(wkt_tessa,"tessa input") {
            using qi::lit;
            using qi::double_;
            using qi::eps;
            using qi::attr;
            using qi::on_error;
            using qi::no_case;
            using phoenix::construct;
            using phoenix::val;
            using namespace qi::labels;

            wkt_tessa = eps > ( wkt_geomcoll
                              | (wkt_polygon > attr(MultiLineString()))
                              ); 

            wkt_geomcoll = no_case[lit("GEOMETRYCOLLECTION")] > lit('(')
                        > wkt_polygon
                        > -(',' > wkt_multilinestring)
                        > lit(')');

            wkt_multilinestring = no_case[lit("MULTILINESTRING")] > lit('(')
                        > (point_list % ',')
                        > lit(')');

            wkt_polygon = no_case[lit("POLYGON")] > lit('(')
                        > (point_list % ',')
                        > lit(')');

            point_list = lit('(')
                       > (point % ',')
                       > lit(')');

            point %= double_ > double_;

            point.name("point");
            point_list.name("point_list");
            wkt_polygon.name("wkt_polygon");
            wkt_geomcoll.name("wkt_geometrycollection");
            wkt_multilinestring.name("wkt_multilinestring");
            wkt_tessa.name("tessa input");
        }
        qi::rule<Iterator,Point(),ascii::space_type> point;
        qi::rule<Iterator,Points(),ascii::space_type> point_list;
        qi::rule<Iterator,Polygon(),ascii::space_type> wkt_polygon;
        qi::rule<Iterator,TessaInput(),ascii::space_type> wkt_geomcoll;
        qi::rule<Iterator,MultiLineString(),ascii::space_type> wkt_multilinestring;
        qi::rule<Iterator,TessaInput(),ascii::space_type> wkt_tessa;
    };

    // helpers for printing error messages
    struct printer {
        typedef boost::spirit::utf8_string string;
        int lineno, colno;
        printer(int lineno, int colno) : lineno(lineno),colno(colno) {}
        void element(string const& tag, string const& value, int) const {
            if( value=="" ) {
                console->error( "Parse error at {}:{}, expected {}", lineno, colno, tag );
            } else {
                console->error( "Parse error at {}:{}, expected {} '{}'", lineno, colno, tag, value );
            }
        }
    };
    void print_error(int lineno, int colno, boost::spirit::info const& what) {
        using boost::spirit::basic_info_walker;
        printer pr(lineno,colno);
        basic_info_walker<printer> walker(pr, what.tag, 0);
        boost::apply_visitor(walker, what.value);
    }

    // the actual parse function
    std::tuple<bool,TessaInput> parse_wkt_polygon( std::string wkt ) {
        TessaInput input;
        // run parser
        using It = boost::spirit::line_pos_iterator<std::string::const_iterator>;
        wkt_grammar<It> grammar;
        It begin( wkt.begin() );
        It end( wkt.end() );
        try {
            // success
            bool success = qi::phrase_parse(begin, end, grammar, ascii::space, input);
            return {success, input};
        } catch (qi::expectation_failure<It> const& x) {
            // fail; print error message
            It lower_bound(wkt.begin());
            It upper_bound(wkt.end());
            int lineno = boost::spirit::get_line(x.first);
            int colno = boost::spirit::get_column(lower_bound,x.first);
            print_error(lineno,colno,x.what_);
            auto lineit = boost::spirit::get_current_line(lower_bound,x.first,upper_bound);
            std::string line{lineit.begin(),lineit.end()};
            console->error( line );
            stringstream indicator;
            for( int i=1; i<colno; ++i ) indicator << "-";
            indicator << "^";
            console->error( indicator.str() );
            return {false,{}};
        }
        // pray for RVO
    }

}

#endif