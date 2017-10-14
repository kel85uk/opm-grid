#include <config.h>

#include <dune/common/version.hh>

#if HAVE_DYNAMIC_BOOST_TEST
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE DistributedCpGridTests
#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>

#include <dune/grid/CpGrid.hpp>


// Warning suppression for Dune includes.
#include <opm/grid/utility/platform_dependent/disable_warnings.h>

#include <dune/geometry/referenceelements.hh>
#include <dune/common/fvector.hh>
#include <dune/common/parallel/variablesizecommunicator.hh>
#if HAVE_DUNE_GRID_CHECKS
// The header below are not installed for dune-grid
// Therefore we need to deactivate testing, if they
// not available
#include <dune/grid/test/checkpartition.hh>
#include <dune/grid/test/checkcommunicate.hh>

#endif

#include <opm/grid/utility/platform_dependent/reenable_warnings.h>


#if HAVE_MPI
class MPIError {
public:
  /** @brief Constructor. */
  MPIError(std::string s, int e) : errorstring(s), errorcode(e){}
  /** @brief The error string. */
  std::string errorstring;
  /** @brief The mpi error code. */
  int errorcode;
};

void MPI_err_handler(MPI_Comm *, int *err_code, ...){
  char *err_string=new char[MPI_MAX_ERROR_STRING];
  int err_length;
  MPI_Error_string(*err_code, err_string, &err_length);
  std::string s(err_string, err_length);
  std::cerr << "An MPI Error ocurred:"<<std::endl<<s<<std::endl;
  delete[] err_string;
  throw MPIError(s, *err_code);
}
#endif

class LoadBalanceGlobalIdDataHandle
{
public:
    LoadBalanceGlobalIdDataHandle(const Dune::CpGrid::GlobalIdSet& gid_set,
                                  const Dune::CpGrid& grid,
                                  std::vector<int>& dist_point_ids,
                                  std::vector<int>& dist_cell_ids)
        : gid_set_(gid_set), grid_(grid), dist_point_ids_(dist_point_ids),
          dist_cell_ids_(dist_cell_ids)
    {}
    typedef int DataType;
    bool fixedsize(int /*dim*/, int /*codim*/)
    {
        return true;
    }

    template<class T>
    std::size_t size(const T&)
    {
        return 1;
    }
    template<class B, class T>
    void gather(B& buffer, const T& t)
    {
        buffer.write(gid_set_.id(t));

    }
    template<class B, class T>
    void scatter(B& buffer, const T& t, std::size_t)
    {
        int gid;
        buffer.read(gid);
        if(T::codimension==3)
            dist_point_ids_[grid_.leafIndexSet().index(t)]=gid;
        if(T::codimension==0)
            dist_cell_ids_[grid_.leafIndexSet().index(t)]=gid;
    }
    bool contains(int dim, int codim)
    {
        return dim==3 && (codim<=1 || codim==3);
    }
private:
    const Dune::CpGrid::GlobalIdSet& gid_set_;
    const Dune::CpGrid& grid_;
    std::vector<int>& dist_point_ids_;
    std::vector<int>& dist_cell_ids_;
};

/// \brief A data handle to use with CpGrid::.cellScatterGatherInterface()
/// that checks the correctness of the global cell index at the receiving
/// end.
class CheckGlobalCellHandle
{
public:
    CheckGlobalCellHandle(const std::vector<int>& sendindex,
                          const std::vector<int>& recvindex)
        : sendindex_(sendindex), recvindex_(recvindex)
    {}

    typedef int DataType;
    bool fixedsize()
    {
        return true;
    }

    template<class T>
    std::size_t size(const T&)
    {
        return 1;
    }
    template<class B>
    void gather(B& buffer, std::size_t i)
    {
        buffer.write(sendindex_[i]);
    }
    template<class B>
    void scatter(B& buffer, const std::size_t& i, std::size_t)
    {
        int gid;
        buffer.read(gid);
        BOOST_REQUIRE(gid==recvindex_[i]);
    }
private:
    const std::vector<int>& sendindex_;
    const std::vector<int>& recvindex_;
};

class GatherGlobalIdDataHandle
{
public:
    GatherGlobalIdDataHandle(const Dune::CpGrid::GlobalIdSet& gathered_gid_set,
                             const Dune::CpGrid::LeafIndexSet& distributed_indexset,
                       std::vector<int>& dist_point_ids,
                       std::vector<int>& dist_cell_ids)
        : gathered_gid_set_(gathered_gid_set), distributed_indexset_(distributed_indexset),
          dist_point_ids_(dist_point_ids),
          dist_cell_ids_(dist_cell_ids)
    {}

    typedef int DataType;
    bool fixedsize(int /*dim*/, int /*codim*/)
    {
        return true;
    }

    template<class T>
    std::size_t size(const T&)
    {
        return 1;
    }
    template<class B, class T>
    void gather(B& buffer, const T& t)
    {
        if(T::codimension==0)
            buffer.write(dist_cell_ids_[distributed_indexset_.index(t)]);
        if(T::codimension==3)
            buffer.write(dist_point_ids_[distributed_indexset_.index(t)]);
    }
    template<class B, class T>
    void scatter(B& buffer, const T& t, std::size_t)
    {
        int gid;
        buffer.read(gid);
        if(gid!=gathered_gid_set_.id(t))
            OPM_THROW(std::runtime_error, "Exspected a different global id");
    }
    bool contains(int dim, int codim)
    {
        return dim==3 && (codim<=1 || codim==3);
    }
private:
    const Dune::CpGrid::GlobalIdSet& gathered_gid_set_;
    const Dune::CpGrid::LeafIndexSet& distributed_indexset_;
    std::vector<int>& dist_point_ids_;
    std::vector<int>& dist_cell_ids_;
};

class DummyDataHandle
{
public:
    typedef double DataType;
    bool fixedsize(int /*dim*/, int /*codim*/)
    {
        return true;
    }

    template<class T>
    std::size_t size(const T&)
    {
        return 1;
    }
    template<class B, class T>
    void gather(B& buffer, const T&)
    {
        buffer.write(100.0);
    }
    template<class B, class T>
    void scatter(B& buffer, const T&, std::size_t s)
    {
        double val;
        //std::cout<<"Scattering ";
        for(std::size_t i=0; i<s; ++i)
        {
            buffer.read(val);
            //  std::cout<<val<<" "<<i<<" ";
        }
        //std::cout<<"to "<<t.index()<<" with codim"<<T::codimension<<std::endl;
    }
    bool contains(int dim, int codim)
    {
        return dim==3 && (codim<=1 || codim==3);
    }
};

BOOST_AUTO_TEST_CASE(distribute)
{

    int m_argc = boost::unit_test::framework::master_test_suite().argc;
    char** m_argv = boost::unit_test::framework::master_test_suite().argv;
    Dune::MPIHelper::instance(m_argc, m_argv);
    int procs=1;
#if HAVE_MPI
    MPI_Errhandler handler;
    MPI_Comm_create_errhandler(MPI_err_handler, &handler);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, handler);
    MPI_Comm_size(MPI_COMM_WORLD, &procs);
#endif
    Dune::CpGrid grid;
    std::array<int, 3> dims={{10, 10, 10}};
    std::array<double, 3> size={{ 1.0, 1.0, 1.0}};

    grid.createCartesian(dims, size);
#if HAVE_MPI
    BOOST_REQUIRE(grid.comm()==MPI_COMM_SELF);
#endif
    std::vector<int> cell_indices, face_indices, point_indices;
    std::vector<Dune::CpGrid::Traits::Codim<0>::Geometry::GlobalCoordinate > cell_centers, face_centers, point_centers;

    typedef Dune::CpGrid::LeafGridView GridView ;
    GridView gridView = grid.leafGridView();

    int cell_size = gridView.size(0);
    int face_size = gridView.size(1);
    int point_size = gridView.size(3);

    typedef GridView :: IndexSet IndexSet;
    const IndexSet& ix = gridView.indexSet();

    if(procs==1)
    {
        typedef GridView :: Codim<0> :: Iterator LeafIterator ;
        for (LeafIterator it = gridView.begin<0>();
             it != gridView.end<0>(); ++it) {
            Dune::GeometryType gt = it->type () ;
            const Dune::ReferenceElement<Dune::CpGrid::ctype, 3>& ref=
                Dune::ReferenceElements<Dune::CpGrid::ctype, 3>::general(gt);

            cell_indices.push_back(ix.index(*it));
            cell_centers.push_back(it->geometry().center());
            typedef GridView :: IntersectionIterator IntersectionIterator;
            for(IntersectionIterator iit=gridView.ibegin(*it),
                    endiit = gridView.iend(*it); iit!=endiit; ++iit)
            {
                //            face_indices.push_back(ix.index(*it->subEntity<1>(iit->indexInInside())));
                face_centers.push_back(iit->geometry().center());
                for(int i=0; i<4; ++i){
                    point_indices.push_back(ix.subIndex(*it, ref.subEntity(iit->indexInInside(),1,i,3), 3));
                    //ref.subEntity(iit->indexInInside(),1,i,dim).geometry().center();
                }
            }
        }
    }
    DummyDataHandle data;

    const Dune::CpGrid::GlobalIdSet& unbalanced_gid_set=grid.globalIdSet();

    grid.communicate(data, Dune::All_All_Interface, Dune::ForwardCommunication);
    grid.loadBalance(data);

    if ( grid.numCells())
    {
        std::array<int,3> ijk;
        grid.getIJK(0, ijk);
    }

#if HAVE_MPI
    // Dune::CpGrid::loadBalance() is non-trivial only if we have MPI
    // *and* if the target Dune platform is sufficiently recent.
    BOOST_REQUIRE(grid.comm()!=MPI_COMM_SELF||MPI_COMM_WORLD==MPI_COMM_SELF);
#endif // HAVE_MPI

    if(procs==1)
    {
        // Check whether the scattered grid is identical to the orinal one.
        BOOST_REQUIRE(cell_size  == gridView.size(0));
        BOOST_REQUIRE(face_size  == gridView.size(1));
        BOOST_REQUIRE(point_size == gridView.size(3));

        int cell_index=0, face_index=0, point_index=0;

        const Dune::CpGrid::LeafIndexSet& ix1 = grid.leafIndexSet();
#if HAVE_MPI
        BOOST_REQUIRE(&ix!=&ix1);
#endif

        for (Dune::CpGrid::Codim<0>::LeafIterator it = grid.leafbegin<0>();
             it != grid.leafend<0>(); ++it) {
            Dune::GeometryType gt = it->type () ;
            const Dune::ReferenceElement<Dune::CpGrid::ctype, 3>& ref=
                Dune::ReferenceElements<Dune::CpGrid::ctype, 3>::general(gt);

            BOOST_REQUIRE(cell_indices[cell_index]==ix1.index(*it));
            BOOST_REQUIRE(cell_centers[cell_index++]==it->geometry().center());
            for(Dune::CpGrid::LeafIntersectionIterator iit=gridView.ibegin(*it),
                    endiit = gridView.iend(*it); iit!=endiit; ++iit)
            {
                //BOOST_REQUIRE(face_indices[face_index]==ix1.index(*it->subEntity<1>(iit->indexInInside())));
                BOOST_REQUIRE(face_centers[face_index++]==iit->geometry().center());
                for(int i=0; i<4; ++i){
                    BOOST_REQUIRE(point_indices[point_index++]==ix1.subIndex(*it, ref.subEntity(iit->indexInInside(),1,i,3), 3));
                    //ref.subEntity(iit->indexInInside(),1,i,dim).geometry().center();
                }
            }
        }
    }else
    {
#if HAVE_DUNE_GRID_CHECKS
        //checkCommunication(grid,-1,Dune::dvverb); // Deactivated as one has to patch cpgrid to support Intersection::geometryInInside and Outside
        checkPartitionType( gridView );
#endif
        std::vector<int> point_ids(grid.leafIndexSet().size(3)), cell_ids(grid.leafIndexSet().size(0));
        LoadBalanceGlobalIdDataHandle lb_gid_data(unbalanced_gid_set,
                                                  grid,
                                                  point_ids,
                                                  cell_ids);
        grid.scatterData(lb_gid_data);
        GatherGlobalIdDataHandle gather_gid_set_data(unbalanced_gid_set,
                                                     grid.leafIndexSet(),
                                                     point_ids,
                                                     cell_ids);
        grid.gatherData(gather_gid_set_data);

    }
}

// A small test that gathers/scatter the global cell indices.
// On the sending side these are sent and on the receiving side
// these are check with the globalCell values.
BOOST_AUTO_TEST_CASE(cellGatherScatterWithMPI)
{

    Dune::CpGrid grid;
    std::array<int, 3> dims={{8, 4, 2}};
    std::array<double, 3> size={{ 8.0, 4.0, 2.0}};
    grid.createCartesian(dims, size);
    typedef Dune::CpGrid::LeafGridView GridView;
    enum{dimWorld = GridView::dimensionworld};

    grid.loadBalance();
    auto global_grid = grid;
    global_grid.switchToGlobalView();

    auto scatter_handle = CheckGlobalCellHandle(global_grid.globalCell(),
                                                grid.globalCell());
    auto gather_handle = CheckGlobalCellHandle(grid.globalCell(),
                                               global_grid.globalCell());
#if HAVE_MPI
    Dune::VariableSizeCommunicator<> scatter_gather_comm(grid.comm(), grid.cellScatterGatherInterface());
    scatter_gather_comm.forward(scatter_handle);
    scatter_gather_comm.backward(gather_handle);
#else
    (void) scatter_handle;
    (void) gather_handle;
#endif
}

BOOST_AUTO_TEST_CASE(intersectionOverlap)
{
    Dune::CpGrid grid;
    std::array<int, 3> dims={{8, 4, 2}};
    std::array<double, 3> size={{ 8.0, 4.0, 2.0}};
    grid.createCartesian(dims, size);
    typedef Dune::CpGrid::LeafGridView GridView;
    GridView gridView(grid.leafGridView());
    enum{dimWorld = GridView::dimensionworld};
    typedef typename GridView::ctype CoordScalar;
    typedef Dune::FieldVector<CoordScalar,dimWorld> GlobalPosition;
    typedef GridView::Codim<0>::Iterator ElementIterator;
    typedef typename GridView::IntersectionIterator IntersectionIterator;

    grid.loadBalance();
    ElementIterator endEIt = gridView.end<0>();
    for (ElementIterator eIt = gridView.begin<0>(); eIt != endEIt; ++eIt) {
        IntersectionIterator isEndIt = gridView.iend(eIt);
        for (IntersectionIterator isIt = gridView.ibegin(eIt); isIt != isEndIt; ++isIt)
        {
            if (isIt->neighbor())
            {
                GlobalPosition distVec = eIt->geometry().center() -
                    isIt->outside()->geometry().center();
                // Make sure that Coordinates of an element and its neighbor are not identical
                BOOST_REQUIRE(distVec.two_norm2()>=1e-8);
            }
        }
    }
}

bool
init_unit_test_func()
{
    return true;
}

int main(int argc, char** argv)
{
    Dune::MPIHelper::instance(argc, argv);
    boost::unit_test::unit_test_main(&init_unit_test_func,
                                     argc, argv);
}
#else
int main () { return 0; }
#endif // #if HAVE_DYNAMIC_BOOST_TEST
