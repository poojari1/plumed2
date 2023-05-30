/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2012-2023 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#include "CLTool.h"
#include "CLToolRegister.h"
#include "tools/Tools.h"
#include "config/Config.h"
#include "core/PlumedMain.h"
#include "core/ActionSet.h"
#include "core/ActionRegister.h"
#include "core/ActionShortcut.h"
#include "core/ActionToPutData.h"
#include "core/ActionWithVirtualAtom.h"
#include "core/ActionWithVector.h"
#include <cstdio>
#include <string>
#include <iostream>

namespace PLMD {
namespace cltools {

//+PLUMEDOC TOOLS show_graph
/*
show_graph is a tool that takes a plumed input and generates a graph showing how
data flows through the action set involved.  

If this tool is invoked without the --force keyword then the way data is passed through the code during the forward pass
through the action is shown.

When the --force keyword is used then the way forces are passed from biases through actions is shown.

\par Examples

The following generates the mermaid file for the input in plumed.dat
\verbatim
plumed show_graph --plumed plumed.dat
\endverbatim

*/
//+ENDPLUMEDOC

class ShowGraph :
  public CLTool
{
public:
  static void registerKeywords( Keywords& keys );
  explicit ShowGraph(const CLToolOptions& co );
  int main(FILE* in, FILE*out,Communicator& pc);
  std::string description()const {
    return "generate a graph showing how data flows through a PLUMED action set";
  }
  std::string getLabel(const Action* a, const bool& amp=false);
  std::string getLabel(const std::string& s, const bool& amp=false );
  void printStyle( const unsigned& linkcount, const Value* v, OFile& ofile );
  void printArgumentConnections( const ActionWithArguments* a, unsigned& linkcount, const bool& force, OFile& ofile );
  void printAtomConnections( const ActionAtomistic* a, unsigned& linkcount, const bool& force, OFile& ofile );
};

PLUMED_REGISTER_CLTOOL(ShowGraph,"show_graph")

void ShowGraph::registerKeywords( Keywords& keys ) {
  CLTool::registerKeywords( keys );
  keys.add("compulsory","--plumed","plumed.dat","the plumed input that we are generating the graph for");
  keys.add("compulsory","--out","graph.md","the dot file containing the graph that has been generated");
  keys.addFlag("--force",false,"print a graph that shows how forces are passed through the actions");
}

ShowGraph::ShowGraph(const CLToolOptions& co ):
  CLTool(co)
{
  inputdata=commandline;
}

std::string ShowGraph::getLabel(const Action* a, const bool& amp) {
  return getLabel( a->getLabel(), amp );
}

std::string ShowGraph::getLabel( const std::string& s, const bool& amp ) {
  if( s.find("@")==std::string::npos ) return s;
  std::size_t p=s.find_first_of("@");
  if( amp ) return "#64;" + s.substr(p+1); 
  return s.substr(p+1);
}

void ShowGraph::printStyle( const unsigned& linkcount, const Value* v, OFile& ofile ) {
  if( v->getRank()>0 && v->hasDerivatives() ) ofile.printf("linkStyle %d stroke:green,color:green;\n", linkcount);
  else if( v->getRank()==1 ) ofile.printf("linkStyle %d stroke:blue,color:blue;\n", linkcount);
  else if ( v->getRank()==2 ) ofile.printf("linkStyle %d stroke:red,color:red;\n", linkcount);
}

void ShowGraph::printArgumentConnections( const ActionWithArguments* a, unsigned& linkcount, const bool& force, OFile& ofile ) {
   if( !a ) return;
   for(const auto & v : a->getArguments() ) {
       if( force && v->forcesWereAdded() ) ofile.printf("%s -- %s --> %s\n", getLabel(a).c_str(), v->getName().c_str(), getLabel(v->getPntrToAction()).c_str() );
       else if( !force ) ofile.printf("%s -- %s --> %s\n", getLabel(v->getPntrToAction()).c_str(),v->getName().c_str(),getLabel(a).c_str() );
       printStyle( linkcount, v, ofile ); linkcount++;
   }
}

void ShowGraph::printAtomConnections( const ActionAtomistic* a, unsigned& linkcount, const bool& force, OFile& ofile ) {
   if( !a ) return;
   for(const auto & d : a->getDependencies() ) {
       ActionToPutData* dp=dynamic_cast<ActionToPutData*>(d);
       if( dp && dp->getLabel()=="posx" ) {
           if( force ) ofile.printf("%s --> MD\n", getLabel(a).c_str() );
           else ofile.printf("MD --> %s\n", getLabel(a).c_str() );
           ofile.printf("linkStyle %d stroke:violet,color:violet;\n", linkcount); linkcount++;
       } else if( dp && dp->getLabel()!="posy" && dp->getLabel()!="posz" && dp->getLabel()!="Masses" && dp->getLabel()!="Charges" ) {
           if( force ) ofile.printf("%s -- %s --> %s\n",getLabel(a).c_str(), getLabel(d).c_str(), getLabel(d).c_str() ); 
           else ofile.printf("%s -- %s --> %s\n", getLabel(d).c_str(),getLabel(d).c_str(),getLabel(a).c_str() );
           printStyle( linkcount, dp->copyOutput(0), ofile ); linkcount++;
           continue;
       } 
       ActionWithVirtualAtom* dv=dynamic_cast<ActionWithVirtualAtom*>(d);
       if( dv ) {
           if( force ) ofile.printf("%s -- %s --> %s\n", getLabel(a).c_str(),getLabel(d).c_str(),getLabel(d).c_str() );
           else ofile.printf("%s -- %s --> %s\n", getLabel(d).c_str(),getLabel(d).c_str(),getLabel(a).c_str() ); 
           ofile.printf("linkStyle %d stroke:violet,color:violet;\n", linkcount); linkcount++;
       }
   }
}

int ShowGraph::main(FILE* in, FILE*out,Communicator& pc) {

  std::string inpt; parse("--plumed",inpt);
  std::string outp; parse("--out",outp);
  bool forces; parseFlag("--force",forces);

  // Create a plumed main object and initilize
  PlumedMain p; int rr=sizeof(double);
  p.cmd("setRealPrecision",&rr);
  double lunit=1.0; p.cmd("setMDLengthUnits",&lunit);
  double cunit=1.0; p.cmd("setMDChargeUnits",&cunit);
  double munit=1.0; p.cmd("setMDMassUnits",&munit);
  p.cmd("setPlumedDat",inpt.c_str());
  p.cmd("setLog",out);
  int natoms=1000000; p.cmd("setNatoms",&natoms);
  p.cmd("init");

  unsigned linkcount=0; OFile ofile; ofile.open(outp);
  if( forces ) {
      unsigned step=1; p.cmd("setStep",step);
      p.cmd("prepareCalc");
      ofile.printf("flowchart BT \n"); 
      for(auto pp=p.getActionSet().rbegin(); pp!=p.getActionSet().rend(); ++pp) {
          const auto & p(pp->get());
          if( p->getName()=="DOMAIN_DECOMPOSITION" || p->getLabel()=="posx" || p->getLabel()=="posy" || p->getLabel()=="posz" || p->getLabel()=="Masses" || p->getLabel()=="Charges" ) continue;        
   
          if(p->isActive()) {
             ActionToPutData* ap=dynamic_cast<ActionToPutData*>(p);
             if( ap ) {
                 ofile.printf("%s{{\"`label=%s \n %s \n`\"}}\n", getLabel(p).c_str(), getLabel(p,true).c_str(), p->writeInGraph().c_str() );
                 continue;
             }
             ActionWithValue* av=dynamic_cast<ActionWithValue*>(p);
             if( !av ) continue ;
             // Now apply the force if there is one
             p->apply();
             bool hasforce=false;
             for(int i=0;i<av->getNumberOfComponents();++i) {
                 if( (av->copyOutput(i))->forcesWereAdded() ) { hasforce=true; break; }
             }
             // This checks for biases
             ActionWithArguments* aaa=dynamic_cast<ActionWithArguments*>(p);
             if( aaa ) {
                for(const auto & v : aaa->getArguments() ) {
                   if( v->forcesWereAdded() ) { hasforce=true; break; }
                }
             }
             if( !hasforce ) continue;
             // Print out the node if we have force on it 
             ofile.printf("%s([\"`label=%s \n %s \n`\"])\n", getLabel(p).c_str(), getLabel(p,true).c_str(), p->writeInGraph().c_str() );
             // Check where this force is being added 
             printArgumentConnections( aaa, linkcount, true, ofile );
             ActionAtomistic* at=dynamic_cast<ActionAtomistic*>(p);
             if( at ) printAtomConnections( at, linkcount, true, ofile );
          }
      }
      ofile.printf("MD{{positions from MD}}\n"); 
      return 0;
  }

  ofile.printf("flowchart TB \n"); ofile.printf("MD{{positions from MD}}\n");
  for(const auto & aa : p.getActionSet() ) {
      Action* a(aa.get()); 
      if( a->getName()=="DOMAIN_DECOMPOSITION" || a->getLabel()=="posx" || a->getLabel()=="posy" || a->getLabel()=="posz" || a->getLabel()=="Masses" || a->getLabel()=="Charges" ) continue;
      ActionToPutData* ap=dynamic_cast<ActionToPutData*>(a);
      if( ap ) {
          ofile.printf("%s{{\"`label=%s \n %s \n`\"}}\n", getLabel(a).c_str(), getLabel(a,true).c_str(), a->writeInGraph().c_str() ); 
          continue;
      }
      ActionShortcut* as=dynamic_cast<ActionShortcut*>(a); if( as ) continue ; 
      ActionWithValue* av=dynamic_cast<ActionWithValue*>(a);
      ActionWithArguments* aaa=dynamic_cast<ActionWithArguments*>(a);
      ActionAtomistic* at=dynamic_cast<ActionAtomistic*>(a);
      ActionWithVector* avec=dynamic_cast<ActionWithVector*>(a);
      // Print out the connections between nodes
      printAtomConnections( at, linkcount, false, ofile );
      printArgumentConnections( aaa, linkcount, false, ofile );
      // Print out the nodes
      if( avec && !avec->actionInChain() ) {
          ofile.printf("subgraph sub%s [%s]\n",getLabel(a).c_str(),getLabel(a).c_str());
          std::vector<std::string> mychain; avec->getAllActionLabelsInChain( mychain );
          for(unsigned i=0; i<mychain.size(); ++i) {
              Action* ag=p.getActionSet().selectWithLabel<Action*>(mychain[i]);
              ofile.printf("%s([\"`label=%s \n %s \n`\"])\n", getLabel(mychain[i]).c_str(), getLabel(mychain[i],true).c_str(), ag->writeInGraph().c_str() );
          }
          ofile.printf("end\n");
      } else if( !av ) {
          ofile.printf("%s(\"`label=%s \n %s \n`\")\n", getLabel(a).c_str(), getLabel(a,true).c_str(), a->writeInGraph().c_str() );
      } else if( !avec ) {
          ofile.printf("%s([\"`label=%s \n %s \n`\"])\n", getLabel(a).c_str(), getLabel(a,true).c_str(), a->writeInGraph().c_str() );
      }
  }
  ofile.close();

  return 0;
}

} // End of namespace
}
