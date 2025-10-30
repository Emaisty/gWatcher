#include<bits/stdc++.h>
using namespace std;
 
// #include<ext/pb_ds/assoc_container.hpp>
// #include<ext/pb_ds/tree_policy.hpp>
// using namespace __gnu_pbds;
// typedef tree<int, null_type, less<int>, rb_tree_tag, tree_order_statistics_node_update> ordered_set; // find_by_order, order_of_key
 
#pragma GCC optimize("O3,unroll-loops,no-stack-protector,fast-math")
 
#define int long long
#define all(x) x.begin(),x.end()
#define rall(x) x.rbegin(),x.rend()
#define sz(x) (int)(x.size())
#define uniq(v) v.erase(unique(all(v)), v.end())
#define ln '\n'
#define sp ' '
#define fi first
#define se second
#define mem1(a) memset(a,-1,sizeof(a))
#define mem0(a) memset(a,0,sizeof(a))
#define yes cout<<"YES"<<ln
#define no cout<<"NO"<<ln
#define pb push_back
#define Mickeyy() ios_base::sync_with_stdio(false); cin.tie(NULL); cout.tie(NULL)
 
typedef pair<int,int> pii;
typedef vector<int> vi;
typedef vector<vi> vvi;
typedef vector<pii> vii;
 
// Forward loop
#define rep2(i, e) for(int i = 0; i <= (int)(e); ++i)
#define rep3(i, s, e) for(int i = (s); i <= (int)(e); ++i)
#define GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define EXPAND(x) x
#define rep(...) EXPAND(GET_MACRO(__VA_ARGS__, rep3, rep2)(__VA_ARGS__))
 
// Reverse loop
#define rev2(i, e) for(int i = (int)(e); i >= 0; --i)
#define rev3(i, s, e) for(int i = (s); i >= (int)(e); --i)
#define GET_MACRO_REV(_1, _2, _3, NAME, ...) NAME
#define EXPAND_REV(x) x
#define rev(...) EXPAND_REV(GET_MACRO_REV(__VA_ARGS__, rev3, rev2)(__VA_ARGS__))
 
// I/O
template<typename T1, typename T2> istream& operator>>(istream& in, pair<T1, T2>& p) { return in >> p.first >> p.second; }
template<typename T1, typename T2> ostream& operator<<(ostream& out, const pair<T1, T2>& p) { return out << "(" << p.first << ", " << p.second << ")"; }
template<typename T> istream& operator>>(istream& in, vector<T>& v) { for (auto& x : v) in >> x; return in; }
template<typename T> ostream& operator<<(ostream& out, const vector<T>& v) { for (const auto& x : v) out << x << " "; return out; }
template<typename T> istream& operator>>(istream& in, vector<vector<T>>& v) { for (auto& row : v) in >> row; return in; }
template<typename T> ostream& operator<<(ostream& out, const vector<vector<T>>& v) { for (const auto& row : v) out << row << '\n'; return out; }
 
#ifndef ONLINE_JUDGE
    #define dbg(x...) do { cerr << "\e[96m" << #x << " = "; _print(x); cerr << "\e[39m" << ln; } while (0)
#else
    #define dbg(x...)
#endif
 
void _print(long long t) {cerr << t;}
void _print(string t) {cerr << '"' << t << '"';}
void _print(const char *t) {cerr << '"' << t << '"';}
void _print(char t) {cerr << '\'' << t << '\'';}
void _print(double t) {cerr << t;}
void _print(long double t) {cerr << t;}
void _print(unsigned long long t) {cerr << t;}
void _print(bool t) {cerr << (t ? 1 : 0);}
template<class T, class V> void _print(pair<T, V> p);
template<class T> void _print(T v);
template<class T, std::size_t... Is> void print_tuple(const T& t, std::index_sequence<Is...>);
template<class... T> void _print(const tuple<T...>& t);
template<class T> void _print(stack<T> s);
template<class T> void _print(queue<T> q);
template<class T> void _print(priority_queue<T> pq);
template<class T, class Container, class Compare> void _print(priority_queue<T, Container, Compare> pq);
template<typename T, typename... V> void _print(T t, V... v) {_print(t); if constexpr (sizeof...(v) > 0) {cerr << ", "; _print(v...);}}
template<class T, class V> void _print(pair<T, V> p) {cerr << "{"; _print(p.first); cerr << ", "; _print(p.second); cerr << "}";}
template<class T> void _print(T v) {cerr << "[ "; for(auto i : v) {_print(i); cerr << " ";} cerr << "]";}
template<class T, std::size_t... Is> void print_tuple(const T& t, std::index_sequence<Is...>) {cerr << "("; (..., (cerr << (Is == 0 ? "" : ", ") << get<Is>(t))); cerr << ")";}
template<class... T> void _print(const tuple<T...>& t) {print_tuple(t, std::make_index_sequence<sizeof...(T)>());}
template<class T> void _print(stack<T> s) { cerr << "[ "; while (!s.empty()) { _print(s.top()); s.pop(); cerr << " "; } cerr << "]"; }
template<class T> void _print(queue<T> q) { cerr << "[ "; while (!q.empty()) { _print(q.front()); q.pop(); cerr << " "; } cerr << "]"; }
template<class T> void _print(priority_queue<T> pq) { cerr << "[ "; while (!pq.empty()) { _print(pq.top()); pq.pop(); cerr << " "; } cerr << "]"; }
template<class T, class Container, class Compare> void _print(priority_queue<T, Container, Compare> pq) { cerr << "[ "; while (!pq.empty()) { _print(pq.top()); pq.pop(); cerr << " "; } cerr << "]"; }
 
// CONSTANTS
constexpr int mod = 1e9 + 7;
constexpr int MAXN = 1e5 + 5;
constexpr long long INF = 1e18;
constexpr long long NINF = -1e18;
 
// MODULAR ARITHMETIC
int add(int a, int b, int m = mod) { return (a + b) % m; }
int sub(int a, int b, int m = mod) { return (a - b + m) % m; }
int mul(int a, int b, int m = mod) { return (a * b) % m; }
int power(int a, int b, int m = mod) { int res = 1; a %= m; while (b > 0) { if (b & 1) res = mul(res, a, m); a = mul(a, a, m); b >>= 1; } return res; }
int mod_inv(int n, int m = mod) { return power(n, m - 2, m); }
 
// COMBINATORICS
int fact[MAXN+1], invFact[MAXN+1];
void precomp_fact() {
    fact[0] = invFact[0] = 1;
    rep(i, 1, MAXN) fact[i] = mul(fact[i-1], i);
    invFact[MAXN] = mod_inv(fact[MAXN]);
    rev(i, MAXN - 1, 0) invFact[i] = mul(invFact[i+1], i+1);
}
int nCr(int n, int r) {
    if (r < 0 || r > n) return 0;
    return mul(fact[n], mul(invFact[r], invFact[n - r]));
}
 
int Mid(int l,int r) { return (l + (r - l) / 2); }
int gcd(int a, int b) { while (b) { a %= b; swap(a, b); } return a; }
int lcm(int a, int b) { return (a / gcd(a, b)) * b; }
 
// const int max_n = 1e7 + 3;
// int dp[max_n];
 
/*
 
bkc kya tha yeh 
chiiii
 
*/

volatile int watched = 0;

std::vector<std::pair<std::pair<int, int>, std::vector<int>>> input_kek = {
    {{6, 3}, {1, 2, 3, 4, 5, 6}},
    {{6, 5}, {3, 1, 6, 5, 2, 4}},
    {{5, 1}, {3, 5, 4, 2, 1}},
    {{6, 3}, {4, 3, 1, 5, 2, 6}},
    {{3, 2}, {3, 2, 1}}
};
 
void solve(int t){
    int n,x;
    n = input_kek[t].first.first;
    x = input_kek[t].first.second;
    std::vector<int> v = input_kek[t].second;
 
    if(is_sorted(all(v))){
        cout<<0<<ln;
        return;
    }
 
    vii ops;
 
    int idx = find(all(v),x) - v.begin();
 
    watched = 1;
    int high = n+1;
    int mid;
 
 
 
    while(high - watched != 1){
        mid = Mid(watched,high);
        dbg(mid);
 
 
        if(v[mid-1] <= x){
            watched = mid;
        }
 
 
        else{
            high = mid; 
        }
 
    }
 
    
    if(v[watched-1] == x){
        cout<<0<<ln;
        return;
    }
 
    else{
        cout<<1<<ln;
        cout<<idx+1<<sp<<watched<<ln;
    }
    
 
 
 
 
}
 
signed main(){
    // precomp_fact();
    Mickeyy();
    int t = 0;
    while(t < 5){
        solve(t);
        t++;
    }
    return 0;
}