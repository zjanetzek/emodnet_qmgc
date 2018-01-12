//
// Created by Ricard Campos (rcampos@eia.udg.edu).
//

#ifndef EMODNET_TOOLS_TILE_BORDER_VERTICES_H
#define EMODNET_TOOLS_TILE_BORDER_VERTICES_H

#include "cgal_defines.h"
#include <vector>


/**
 * Since for the border vertices one of the coordinates can be deduced depending on which border vertex they are, we
 * just store the variable coordinate and the height measure
 */
struct BorderVertex {
    double coord ;
    double height ;

    BorderVertex( const double& c, const double& h ) : coord(c), height(h) {}
};



class TileBorderVertices
{
public:
    TileBorderVertices()
            : m_easternVertices()
            , m_westernVertices()
            , m_northernVertices()
            , m_southernVertices()
            , m_lifeCounter(0) {}

    TileBorderVertices( const std::vector<BorderVertex>& easternVertices,
                        const std::vector<BorderVertex>& westernVertices,
                        const std::vector<BorderVertex>& northernVertices,
                        const std::vector<BorderVertex>& southernVertices,
                        const int& lifeCounter = 4 ) // The life counter will be 4 for those tiles not in the zoom borders
            : m_easternVertices(easternVertices)
            , m_westernVertices(westernVertices)
            , m_northernVertices(northernVertices)
            , m_southernVertices(southernVertices)
            , m_lifeCounter(lifeCounter) {}

    // Each time we consult for a border, we update the number of times it has been consulted by decreasing the lifeCounter.
    // When it gets to zero, the information in this object will not be needed anymore and can be deleted (responsability of the cache object)
    std::vector<BorderVertex> getEasternVertices() { m_lifeCounter-- ; return m_easternVertices ; }
    std::vector<BorderVertex> getWesternVertices() { m_lifeCounter-- ; return m_westernVertices ; }
    std::vector<BorderVertex> getNorthernVertices() { m_lifeCounter-- ; return m_northernVertices ; }
    std::vector<BorderVertex> getSouthernVertices() { m_lifeCounter-- ; return m_southernVertices ; }

    bool isAlive() { return m_lifeCounter > 0 ; }

private:
    std::vector<BorderVertex> m_easternVertices ;
    std::vector<BorderVertex> m_westernVertices ;
    std::vector<BorderVertex> m_northernVertices ;
    std::vector<BorderVertex> m_southernVertices ;
    int m_lifeCounter ;
};

#endif //EMODNET_TOOLS_TILE_BORDER_VERTICES_H