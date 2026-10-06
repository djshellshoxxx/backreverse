#include "backreverse/PatternEngine.h"
#include <algorithm>
#include <cctype>
#include <numeric>
#include <random>
#include <sstream>
#include <string>

namespace br {
std::vector<std::size_t> buildOrder(OrderMode mode,std::size_t count,std::uint64_t seed,const std::vector<int>& userPattern){
    std::vector<std::size_t> out(count);std::iota(out.begin(),out.end(),0);if(count==0)return out;
    switch(mode){
        case OrderMode::Sequential:break;
        case OrderMode::ReverseOrder:std::reverse(out.begin(),out.end());break;
        case OrderMode::Random:{std::mt19937_64 rng(seed);std::shuffle(out.begin(),out.end(),rng);break;}
        case OrderMode::ShuffleNoRepeat:{std::mt19937_64 rng(seed);std::shuffle(out.begin(),out.end(),rng);break;}
        case OrderMode::PingPong:{std::vector<std::size_t> p;p.reserve(count==1?1:count*2-2);for(std::size_t i=0;i<count;++i)p.push_back(i);if(count>1)for(std::size_t i=count-2;i>0;--i)p.push_back(i);return p;}
        case OrderMode::OddsThenEvens:{out.clear();for(std::size_t i=1;i<count;i+=2)out.push_back(i);for(std::size_t i=0;i<count;i+=2)out.push_back(i);break;}
        case OrderMode::EvensThenOdds:{out.clear();for(std::size_t i=0;i<count;i+=2)out.push_back(i);for(std::size_t i=1;i<count;i+=2)out.push_back(i);break;}
        case OrderMode::RotateLeft:std::rotate(out.begin(),out.begin()+1,out.end());break;
        case OrderMode::RotateRight:std::rotate(out.begin(),out.end()-1,out.end());break;
        case OrderMode::UserPattern:{
            out.clear();
            for(int v:userPattern){
                if(v==-1)out.push_back(RestChunk);
                else if(v>=0&&static_cast<std::size_t>(v)<count)out.push_back(static_cast<std::size_t>(v));
            }
            if(out.empty()){out.resize(count);std::iota(out.begin(),out.end(),0);}
            break;
        }
    }
    return out;
}

static std::string trim(std::string s){
    auto notSpace=[](unsigned char ch){return !std::isspace(ch);};
    s.erase(s.begin(),std::find_if(s.begin(),s.end(),notSpace));
    s.erase(std::find_if(s.rbegin(),s.rend(),notSpace).base(),s.end());
    return s;
}

std::vector<int> parseUserPattern(const std::string& text,std::size_t chunkCount,std::uint64_t seed){
    std::vector<int> out;if(chunkCount==0)return out;
    std::string normalized=text;for(char& ch:normalized)if(ch==';'||ch=='\n'||ch=='\t')ch=',';
    std::stringstream ss(normalized);std::string token;std::mt19937_64 rng(seed);int last=0;
    while(std::getline(ss,token,',')){
        token=trim(token);if(token.empty())continue;
        int repeat=1;int probability=100;

        auto at=token.find('@');
        if(at!=std::string::npos){
            try{probability=std::clamp(std::stoi(token.substr(at+1)),0,100);}catch(...){probability=100;}
            token=token.substr(0,at);
        }
        auto star=token.find('*');
        if(star!=std::string::npos){
            try{repeat=std::clamp(std::stoi(token.substr(star+1)),1,1024);}catch(...){repeat=1;}
            token=token.substr(0,star);
        }
        token=trim(token);
        std::string upper=token;std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char ch){return static_cast<char>(std::toupper(ch));});
        int value=-2;
        if(upper=="REST"||upper=="R")value=-1;
        else{
            try{
                if(!token.empty()&&(token[0]=='+'||token[0]=='-')){
                    long long delta=std::stoll(token); long long v=static_cast<long long>(last)+delta;
                    v%=static_cast<long long>(chunkCount);if(v<0)v+=static_cast<long long>(chunkCount);value=static_cast<int>(v);
                }else{
                    long long v=std::stoll(token);if(v>=0)value=static_cast<int>(static_cast<std::size_t>(v)%chunkCount);
                }
            }catch(...){value=-2;}
        }
        if(value==-2)continue;
        if(value>=0)last=value;
        for(int i=0;i<repeat;++i){
            const bool keep=static_cast<int>(rng()%100)<probability;
            out.push_back(keep?value:-1);
        }
    }
    return out;
}
}
