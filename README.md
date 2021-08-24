# Proxy
A web proxy in C capable of accepting HTTP 1.0 requests with pre-threading concurrency, thread-safe LRU cache
1. Description
  a. Vanilla proxy
    I start up with building a web proxy as is outlined in https://github.com/xqfqhu/15-213-Lab-Code/blob/master/Lab7_proxylab/proxylab.pdf, which is the       last project of cmu cs 15-213 course. This vanilla proxy features pre-threading concurrency and thread-safe LRU cache. The most challenging part is the     concurrency problem: how to protect shared resources (cache in this case)? how to avoid race condition/deadlock/other synchronization programs. I           handled this problem using mutex. Another challenging part is to prevent the proxy from exiting because of signals. I handled this problem by trapping     the signals.
  b. Improvements I have implemented
    Despite all the possible areas of improvement, I focused on improving (1) speed (2) readability (3) security of my proxy, which I found most               interesting
    (1) Improvement 1: speed up surfing by adding gzip compression
    (2) Improvement 2: Improved readability by filtering banner advertisements and pop-up windows
    (3) Improvement 3: Improved privacy by reporting and blocking cookies
  c. Improvements I'm looking forward to conducting
2. Metrics and performance improvement
3. Demo

