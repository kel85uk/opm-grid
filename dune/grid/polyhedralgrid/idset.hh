// -*- mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=2 sw=2 sts=2:
#ifndef DUNE_POLYHEDRALGRID_IDSET_HH
#define DUNE_POLYHEDRALGRID_IDSET_HH

#include <dune/grid/common/indexidset.hh>

namespace Dune
{

  // PolyhedralGridIdSet
  // -----------

  template< int dim, int dimworld >
  class PolyhedralGridIdSet
      : public IdSet< PolyhedralGrid< dim, dimworld >, PolyhedralGridIdSet< dim, dimworld >, /*IdType=*/int >
  {
  public:
    typedef PolyhedralGrid<  dim, dimworld > Grid;
    typedef typename std::remove_const< Grid >::type::Traits Traits;
    typedef typename Traits::Index  IdType;

    typedef PolyhedralGridIdSet< dim, dimworld > This;
    typedef IdSet< Grid, PolyhedralGridIdSet< dim, dimworld >, IdType > Base;

    PolyhedralGridIdSet (const Grid& grid)
        : grid_(grid)
    {}

    //! id meethod for entity and specific codim
    template< int codim >
    IdType id ( const typename Traits::template Codim< codim >::Entity &entity ) const
    {
      const int index = entity.seed().index();
      if (codim == 0)
        return grid_.globalCell()[ index ];
      else
        return index;
    }

#if ! DUNE_VERSION_NEWER(DUNE_GRID,2,4)
    //! id meethod for entity and specific codim
    template< int codim >
    IdType id ( const typename Traits::template Codim< codim >::EntityPointer &entityPointer ) const
    {
      return id( *entityPointer );
    }
#endif

    //! id method of all entities
    template< class Entity >
    IdType id ( const Entity &entity ) const
    {
      return id< Entity::codimension >( entity );
    }

    //! id method of all entities
    template< class IntersectionImpl >
    IdType id ( const Dune::Intersection< const Grid, IntersectionImpl >& intersection ) const
    {
      return Grid::getRealImplementation( intersection ).id();
    }

    //! subId method for entities
    template< class Entity >
    IdType subId ( const Entity &entity, int i, unsigned int codim ) const
    {
      if( codim == 0 )
        return id( entity );
      else if ( codim == 1 )
        return id( entity.template subEntity< 1 >( i ) );
      else if ( codim == dim )
        return id( entity.template subEntity< dim >( i ) );
      else
      {
        DUNE_THROW(NotImplemented,"codimension not available");
        return IdType( -1 );
      }
    }

  protected:
    const Grid& grid_;
  };

} // namespace Dune

#endif // #ifndef DUNE_POLYHEDRALGRID_IDSET_HH
