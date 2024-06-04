from numpy import cos, pi
arr = []
alpha = 0.46
N=256
for n in range(N):
    value = (1-alpha)-alpha*cos(2*pi*n/(N-1))
    arr.append(int(value*N))

print(arr)