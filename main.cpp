#include <iostream>
#include <thread>
#include <vector>
#include <ctime>
#include <string>
#include <cstdio>
#include <cstdlib>

#include "ReadParser.h"

using namespace std;

struct Bases {
    uint32_t A, C, G, T;
};


int main(int argc, char* argv[]) {
    if (argc < 5) {
        // cerr << "usage: test np nt chunk_size fa1 fb1 ... fa2 fb2 ..."<< endl;
        cerr << "usage: test np nt chunk_size fq1 fq2 ..."<< endl;
        return 1;
    }
    int numFiles = argc - 4;
    // if (numFiles % 2 != 0) {
    //     cerr << "you must provide an even number of files!\n";
    //     return 1;
    // }

    int np = atoi(argv[1]);
    int nt = atoi(argv[2]);
    size_t chunk_size = atoi(argv[3]);

    vector<vector<string>> input_files;
    // size_t numPairs = numFiles / 2;
    // for (size_t i = 0; i < numPairs; ++i) {
    //     vector<string> read_pairs;
    //     read_pairs.push_back(argv[i + 4]);
    //     read_pairs.push_back(argv[i + 4 + numPairs]);
    //     input_files.push_back(read_pairs);
    // }
    for (int i = 0; i < numFiles; ++i) input_files.push_back(vector<string>(1, argv[i+4]));

    time_t start_, end_;

    start_ = time(NULL);

    ReadParser *parser = new ReadParser(input_files, nt, np, chunk_size);

    vector<thread> readers;
    vector<Bases> counters(nt, {0, 0, 0, 0});
    atomic<size_t> ctr{0};
    for (int i = 0; i < nt; ++i) {
        readers.emplace_back([&, i]() {

            parser->start();

            auto rg = parser->getReadGroup();
            size_t lctr{0};
            size_t pctr{0};
            while (true) {

                if (parser->refill(rg)) {
                    for (auto& read_tuple : rg) {
                        ++lctr;

                        for (int j = 0; j < read_tuple.mates.size(); ++j) {
                            for (size_t k = 0; k < read_tuple[j].seq.length(); ++k) {
                                char c = read_tuple[j].seq[k];
                                switch (c) {
                                    case 'A':
                                        counters[i].A++;
                                        break;
                                    case 'C':
                                        counters[i].C++;
                                        break;
                                    case 'G':
                                        counters[i].G++;
                                        break;
                                    case 'T':
                                        counters[i].T++;
                                        break;
                                    default:
                                        break;
                                }
                            }
                        }
                    }

                    ctr += (lctr - pctr);
                    pctr = lctr;
                    if (lctr > 1000000) {
                        lctr = 0;
                        pctr = 0;
                        cout << "parsed " << ctr << " read pairs.\n";

                        //parser->stop();
                        //readers[i].detach();




                    }
                } else {
                    //parser->stop();
                    //std::cout<<"here 96 main"<<std::endl;
                    break;
                }

            }
          // parser->stop();
          //  std::cout<<"here 101 main"<<std::endl;

        });

      //  std::cout<<"here 103 main"<<std::endl;
        //parser->stop();
    }
   // std::cout<<"here 105 main"<<std::endl;
    for (auto& t : readers) {
        //std::cout<<"here 107 main"<<std::endl;
        t.join();
    }
    //std::cout<<"here 110 main"<<std::endl;
   // parser->stop();
    //std::cout<<"here 112 main"<<std::endl;

   // delete parser;
   // std::cout<<"here 115 main"<<std::endl;

    Bases b = {0, 0, 0, 0};
    for (int i = 0; i < nt; ++i) {
       // std::cout<<"here 119 main"<<std::endl;
        b.A += counters[i].A;
        b.C += counters[i].C;
        b.G += counters[i].G;
        b.T += counters[i].T;
    }
    cerr << "\n";
    cerr << "Parsed " << ctr << " total read pairs.\n";
    cerr << "\n#A = " << b.A << '\n';
    cerr << "#C = " << b.C << '\n';
    cerr << "#G = " << b.G << '\n';
    cerr << "#T = " << b.T << '\n';


    end_ = time(NULL);
    printf("Time spent = %.2fs.\n", difftime(end_, start_));

    return 0;
}
