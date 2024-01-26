
#include "art/Framework/Core/InputSourceMacros.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/SharedAnalyzer.h"
#include "art/Framework/IO/Sources/Source.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/fwd.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "canvas/Persistency/Common/Assns.h"
#include "canvas/Persistency/Provenance/canonicalProductName.h"

#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Core/ProductRegistryHelper.h"
#include "art/Framework/IO/Sources/SourceHelper.h"

#include "art/Framework/IO/Sources/put_product_in_principal.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "canvas/Persistency/Provenance/EventID.h"
#include "canvas/Persistency/Provenance/RunID.h"
#include "canvas/Persistency/Provenance/SubRunID.h"

#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/ConfigurationTable.h"

#include "larcoreobj/SimpleTypesAndConstants/RawTypes.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"

#define private public
#include "hepnos.hpp"
#undef private

#include "../serialization/eid_serialization.h"
#include "../serialization/hit_serialization.h"
#include "../serialization/opdetwaveform_serialization.h"
#include "../serialization/rawdigit_serialization.h"
#include "../serialization/spacepoint_serialization.h"
#include "../serialization/wire_serialization.h"

#include "EID.h"
#include "HepnosDataStore.h"
#include "Timestamp.h"
#include <boost/serialization/utility.hpp>
#include <tuple>

using namespace std;
namespace hepnos {
  template <typename A, typename B>
  using Assns = std::vector<std::pair<hepnos::Ptr<A>, hepnos::Ptr<B>>>;

  std::tuple<std::string, std::string, std::string>
  splitTag(std::string const& inputtag)
  {
    // InputTag: label = 'daq0', instance = 'PHYSCRATEDATATPCEE', process =
    // 'DetSim' For clarity I am defining all these variables
    art::InputTag a_inputtag;
    decode(inputtag, a_inputtag);

    std::string label = a_inputtag.label();
    std::string instance = a_inputtag.instance();
    std::string process = a_inputtag.process();
    return std::make_tuple(label, instance, process);
  }

  template <typename T>
  std::unique_ptr<std::vector<T>>
  read_product(hepnos::Event& event,
               std::string const& module_label,
               std::string const& instancename,
               std::string const& processname,
               bool strict)
  {
    art::InputTag const tag(module_label, instancename, processname);
    std::vector<T> products;
    std::unique_ptr<std::vector<T>> products_uptr;

    {
      std::function<void(void)> f = [&]() {
        auto ret = event.load(tag.encode(), products);
        /* the following does not need to be here, but I couldn't figure out
           the type of ret, so it is here for now */
        if (ret)
          products_uptr = std::make_unique<std::vector<T>>(products);
        return;
      };
      art::ServiceHandle<icaruswf::HepnosDataStore> h;
      h->set_work_function(f);
      h->set_work_state();
      h->wait();
      return products_uptr;
    }
    if (strict) {
      throw art::Exception(art::errors::ProductNotFound)
        << "Product not read for tag: " << tag << ", "
        << art::TypeID{products}.friendlyClassName() << "\n";
    }
    return {};
  }

  template <typename T>
  art::ProductID
  create_productID(std::vector<T> const& t,
                   std::string const& module_label,
                   std::string const& instance_name = "")
  {
    auto const product_name =
      art::canonicalProductName(art::TypeID{t}.friendlyClassName(),
                                module_label,
                                instance_name,
                                art::Globals::instance()->processName());
    return art::ProductID{product_name};
  }

  template <typename A, typename B>
  std::unique_ptr<art::Assns<A, B>>
  read_assns(hepnos::Event& event,
             art::EventPrincipal*& outE,
             std::vector<A> const& a_col,
             std::vector<B> const& b_col,
             std::string const& a_module_label,
             std::string const& b_module_label)
  {
    art::InputTag const tag(a_module_label, "", "");
    std::vector<std::pair<hepnos::Ptr<A>, hepnos::Ptr<B>>> assns;
    {
      std::function<void(void)> f = [&]() { event.load(tag.encode(), assns); };
      art::ServiceHandle<icaruswf::HepnosDataStore> h;
      h->set_work_function(f);
      h->set_work_state();
      h->wait();
    }
    auto const a_pid = create_productID<A>(a_col, a_module_label, "");
    auto const b_pid = create_productID<B>(b_col, b_module_label, "");
    art::Assns<A, B> art_assns;
    for (auto const& [a, b] : assns) {
      art_assns.addSingle(
        art::Ptr<A>(a_pid, a.m_index, outE->productGetter(a_pid)),
        art::Ptr<B>(b_pid, b.m_index, outE->productGetter(b_pid)));
    }
    return std::make_unique<art::Assns<A, B>>(art_assns);
  }

  bool
  read_daq(hepnos::Event& event, bool strict, art::EventPrincipal*& outE)
  {
    std::vector<ProductID> prodIds;
    {
      std::function<void(void)> f = [&]() { prodIds = event.listProducts(""); };
      art::ServiceHandle<icaruswf::HepnosDataStore> h;
      h->set_work_function(f);
      h->set_work_state();
      h->wait();
    }

    hepnos::UUID datasetId;
    hepnos::RunNumber run;
    hepnos::SubRunNumber subrun;
    hepnos::EventNumber hepnosevent;
    std::string hepnostag;
    std::string type;
    for (auto p : prodIds) {
      p.unpackInformation(
        &datasetId, &run, &subrun, &hepnosevent, &hepnostag, &type);
      auto [label, instance, process] = hepnos::splitTag(hepnostag);
      std::cout << "Input Source: " << label << ", " << instance << ", "
                << process << "\n";
      if (type ==
          "std::vector<raw::RawDigit, std::allocator<raw::RawDigit> >") {
        auto prod = hepnos::read_product<raw::RawDigit>(
          event, label, instance, process, strict);
        art::put_product_in_principal(std::move(prod), *outE, label, instance);
      }
      if (type == "std::vector<raw::OpDetWaveform, "
                  "std::allocator<raw::OpDetWaveform> >") {
        auto prod = hepnos::read_product<raw::OpDetWaveform>(
          event, label, instance, process, strict);
        art::put_product_in_principal(std::move(prod), *outE, label, instance);
      }
    }
    return true;
  }

  bool
  read_wires(hepnos::Event& event, bool strict, art::EventPrincipal*& outE)
  {
    auto wires0 = hepnos::read_product<recob::Wire>(
      event, "roifinder", "PHYSCRATEDATATPCEE", "SignalProcessing", strict);
    auto wires1 = hepnos::read_product<recob::Wire>(
      event, "roifinder", "PHYSCRATEDATATPCEW", "SignalProcessing", strict);
    auto wires2 = hepnos::read_product<recob::Wire>(
      event, "roifinder", "PHYSCRATEDATATPCWE", "SignalProcessing", strict);
    auto wires3 = hepnos::read_product<recob::Wire>(
      event, "roifinder", "PHYSCRATEDATATPCWW", "SignalProcessing", strict);
    art::put_product_in_principal(
      std::move(wires0), *outE, "roifinder", "PHYSCRATEDATATPCEE");
    art::put_product_in_principal(
      std::move(wires1), *outE, "roifinder", "PHYSCRATEDATATPCEW");
    art::put_product_in_principal(
      std::move(wires2), *outE, "roifinder", "PHYSCRATEDATATPCWE");
    art::put_product_in_principal(
      std::move(wires3), *outE, "roifinder", "PHYSCRATEDATATPCWW");
    return true;
  }

  bool
  read_hits(hepnos::Event& event, bool strict, art::EventPrincipal*& outE)
  {
    auto hits0 = hepnos::read_product<recob::Hit>(
      event, "gaushitTPCEE", "", "HitFinding", strict);
    auto hits1 = hepnos::read_product<recob::Hit>(
      event, "gaushitTPCEW", "", "HitFinding", strict);
    auto hits2 = hepnos::read_product<recob::Hit>(
      event, "gaushitTPCWE", "", "HitFinding", strict);
    auto hits3 = hepnos::read_product<recob::Hit>(
      event, "gaushitTPCWW", "", "HitFinding", strict);
    art::put_product_in_principal(std::move(hits0), *outE, "gaushitTPCEE", "");
    art::put_product_in_principal(std::move(hits1), *outE, "gaushitTPCEW", "");
    art::put_product_in_principal(std::move(hits2), *outE, "gaushitTPCWE", "");
    art::put_product_in_principal(std::move(hits3), *outE, "gaushitTPCWW", "");
    // art::put_product_in_principal(std::move(detwave), *outE, "opdaq", "");
    return true;
  }

  bool
  read_assns_hits_wires(hepnos::Event& event,
                        bool strict,
                        art::EventPrincipal*& outE)
  {
    std::vector<recob::Hit> hits;
    std::vector<recob::Wire> wires;
    auto hits_wires_0 = hepnos::read_assns<recob::Wire, recob::Hit>(
      event, outE, wires, hits, "gaushitTPCEE", "gaushitTPCEE");
    auto hits_wires_1 = hepnos::read_assns<recob::Wire, recob::Hit>(
      event, outE, wires, hits, "gaushitTPCEW", "gaushitTPCEW");
    auto hits_wires_2 = hepnos::read_assns<recob::Wire, recob::Hit>(
      event, outE, wires, hits, "gaushitTPCWE", "gaushitTPCWE");
    auto hits_wires_3 = hepnos::read_assns<recob::Wire, recob::Hit>(
      event, outE, wires, hits, "gaushitTPCWW", "gaushitTPCWW");
    art::put_product_in_principal(
      std::move(hits_wires_0), *outE, "gaushitTPCEE", "");
    art::put_product_in_principal(
      std::move(hits_wires_1), *outE, "gaushitTPCEW", "");
    art::put_product_in_principal(
      std::move(hits_wires_2), *outE, "gaushitTPCWE", "");
    art::put_product_in_principal(
      std::move(hits_wires_3), *outE, "gaushitTPCWW", "");
    return true;
  }
}

namespace icaruswf {
  class LoadbalancingInput {
  public:
    template <typename T>
    void
    reconstitutes(std::string const& moduleLabel,
                  std::string const& instanceName,
                  std::string const& processName)
    {
      art::BranchDescription const bd{
        art::InEvent,
        moduleLabel,
        processName,
        art::TypeID{typeid(T)}.className(),
        instanceName,
        {},
        {},
        art::BranchDescription::Transients::PresentFromSource,
        false,
        false};
      productList_->try_emplace(art::BranchKey{bd}, bd);
    }

    LoadbalancingInput(fhicl::ParameterSet const& p,
                       art::ProductRegistryHelper& rh,
                       art::SourceHelper const& pm)
      : pm_(pm)
      , datastore_{art::ServiceHandle<HepnosDataStore>()->getStore()}
      , inputProcessname_(p.get<std::string>("processToRead"))
      , maxEvents_(p.get<int>("maxEvents", -1))
      , nSkip_(p.get<std::size_t>("skipEvents", -1ull))
      , firstRun_(p.get<unsigned>("firstRun", -1u))
      , firstSubRun_(p.get<unsigned>("firstSubRun", -1u))
      , firstEvent_(p.get<unsigned>("firstEvent", -1u))
    {
      {
        std::function<void(void)> f = [&]() {
          queue_ = datastore_.openQueue<EID>(inputProcessname_,
                                             hepnos::QueueAccessMode::CONSUMER);
          return;
        };
        this->run_hepnos_func(f);
      }

      if (inputProcessname_ == "DetSim") {
        rh.reconstitutes<std::vector<raw::RawDigit>, art::InEvent>(
          "daq0", "PHYSCRATEDATATPCEE");
        rh.reconstitutes<std::vector<raw::RawDigit>, art::InEvent>(
          "daq1", "PHYSCRATEDATATPCEW");
        rh.reconstitutes<std::vector<raw::RawDigit>, art::InEvent>(
          "daq2", "PHYSCRATEDATATPCWE");
        rh.reconstitutes<std::vector<raw::RawDigit>, art::InEvent>(
          "daq3", "PHYSCRATEDATATPCWW");
        rh.reconstitutes<std::vector<raw::OpDetWaveform>, art::InEvent>("opdaq",
                                                                        "");
      } else if (inputProcessname_ == "SignalProcessing") {
        rh.reconstitutes<std::vector<recob::Wire>, art::InEvent>(
          "roifinder", "PHYSCRATEDATATPCEE");
        rh.reconstitutes<std::vector<recob::Wire>, art::InEvent>(
          "roifinder", "PHYSCRATEDATATPCEW");
        rh.reconstitutes<std::vector<recob::Wire>, art::InEvent>(
          "roifinder", "PHYSCRATEDATATPCWE");
        rh.reconstitutes<std::vector<recob::Wire>, art::InEvent>(
          "roifinder", "PHYSCRATEDATATPCWW");
        rh.reconstitutes<std::vector<raw::OpDetWaveform>, art::InEvent>("opdaq",
                                                                        "");
      } else if (inputProcessname_ == "HitFinding") {
        rh.reconstitutes<std::vector<recob::Hit>, art::InEvent>("gaushitTPCEE",
                                                                "");
        rh.reconstitutes<std::vector<recob::Hit>, art::InEvent>("gaushitTPCEW",
                                                                "");
        rh.reconstitutes<std::vector<recob::Hit>, art::InEvent>("gaushitTPCWE",
                                                                "");
        rh.reconstitutes<std::vector<recob::Hit>, art::InEvent>("gaushitTPCWW",
                                                                "");
        rh.reconstitutes<art::Assns<recob::Hit, recob::Wire, void>,
                         art::InEvent>("gaushitTPCEE", "");
        rh.reconstitutes<art::Assns<recob::Hit, recob::Wire, void>,
                         art::InEvent>("gaushitTPCEW", "");
        rh.reconstitutes<art::Assns<recob::Hit, recob::Wire, void>,
                         art::InEvent>("gaushitTPCWE", "");
        rh.reconstitutes<art::Assns<recob::Hit, recob::Wire, void>,
                         art::InEvent>("gaushitTPCWW", "");
      } else {
        throw art::Exception(art::errors::Configuration)
          << "Incorrect value of processToRead supplied: " << inputProcessname_
          << '\n';
        ;
      }
    }

    void
    run_hepnos_func(std::function<void(void)> work)
    {
      art::ServiceHandle<icaruswf::HepnosDataStore> h;
      h->set_work_function(work);
      h->set_work_state();
      h->wait();
      return;
    }

    ~LoadbalancingInput()
    {
      /* close the queue */
      {
        std::function<void(void)> f = [&]() {
          queue_.close();
          return;
        };
        this->run_hepnos_func(f);
      }
      /* reset all hepnos objects, for they may contain
         stray shared_ptrs to datastore making destruction
         tricky */
      {
        std::function<void(void)> f = [&]() {
          r_ = hepnos::RunNumber{};
          sr_ = hepnos::SubRunNumber{};
          dataset_ = hepnos::DataSet{};
          return;
        };
        this->run_hepnos_func(f);
      }
    }

    bool readNext(art::RunPrincipal* const& inR,
                  art::SubRunPrincipal* const& inSR,
                  art::RunPrincipal*& outR,
                  art::SubRunPrincipal*& outSR,
                  art::EventPrincipal*& outE);

    void
    readFile(std::string const& dsname, art::FileBlock*& fb)
    {
      auto dataset_name = dsname;
      {
        std::function<void(void)> f = [&]() {
          dataset_ = datastore_.root()[dataset_name];
          return;
        };
        this->run_hepnos_func(f);
      }
      fb =
        new art::FileBlock{art::FileFormatVersion{1, "HepnosSource"}, dsname};
      size_t i = 0;
    }

    void
    closeCurrentFile()
    {}

  private:
    hepnos::DataStore& datastore_;
    hepnos::DataSet dataset_;
    art::SourceHelper const& pm_;
    std::unique_ptr<art::ProductList> productList_{
      std::make_unique<art::ProductList>()};
    std::string inputProcessname_;
    hepnos::Queue queue_;
    hepnos::RunNumber r_ = -1ull;
    hepnos::SubRunNumber sr_ = -1ull;
    int maxEvents_;
    size_t nSkip_;
    int nEvents_ = 0;
    unsigned firstRun_;
    unsigned firstSubRun_;
    unsigned firstEvent_;
  };

  bool
  LoadbalancingInput::readNext(art::RunPrincipal* const& inR,
                               art::SubRunPrincipal* const& inSR,
                               art::RunPrincipal*& outR,
                               art::SubRunPrincipal*& outSR,
                               art::EventPrincipal*& outE)
  {
    if (nEvents_ == maxEvents_)
      return false;
    // pop event ID of the queue
    // how do we know we are done?
    EID eid;
    bool got_new_event = false;
    {
      std::function<void(void)> f = [&]() {
        got_new_event = queue_.pop(eid); // need to add a check here
        return;
      };
      this->run_hepnos_func(f);
    }
    if (!got_new_event)
      return false;
    auto r = eid.run;
    auto sr = eid.subrun;
    auto ev = eid.event;
    art::Timestamp ts;
    std::cout << "Event: " << ev << "\n";
    if (r != r_) {
      outR = pm_.makeRunPrincipal(r, ts);
      outSR = pm_.makeSubRunPrincipal(r, sr, ts);
      r_ = r;
      sr_ = sr;
    } else if (sr != sr_) {
      outSR = pm_.makeSubRunPrincipal(r, sr, ts);
      sr_ = sr;
    }
    outE = pm_.makeEventPrincipal(r, sr, ev, ts, false);

    hepnos::Event event;
    {
      std::function<void(void)> f = [&]() {
        auto run = dataset_.createRun(r);
        auto subrun = run.createSubRun(sr);
        event = subrun.createEvent(ev);
      };
      this->run_hepnos_func(f);
    }

    auto const strict = true;
    // check whats in store
    auto status = true;
    if (inputProcessname_ == "DetSim") {
      status = hepnos::read_daq(event, strict, outE);
    } else if (inputProcessname_ == "SignalProcessing") {
      status = hepnos::read_wires(event, strict, outE);
    } else if (inputProcessname_ == "HitFinding") {
      status = hepnos::read_hits(event, strict, outE);
    } else {
      throw art::Exception(art::errors::LogicError)
        << "Impossible value of inputPrcessname_: " << inputProcessname_
        << '\n';
      ;
    }
    {
      std::function<void(void)> f = [&]() { ++nEvents_; };
      this->run_hepnos_func(f);
    }

    art::ServiceHandle<icaruswf::Timestamp> h;
    h->updateEndOfRead();

    return true;
  }
}

namespace art {
  // We don't want the file services: we must say so by specializing the //
  // template *before* specifying the typedef.
  template <>
  struct Source_wantFileServices<icaruswf::LoadbalancingInput> {
    static constexpr bool value{false};
  };
} // namespace art
DEFINE_ART_INPUT_SOURCE(art::Source<icaruswf::LoadbalancingInput>);
