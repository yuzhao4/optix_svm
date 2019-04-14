import numpy as np

def projection(point_coords):
    if len(point_coords) != 3:
        print("3 dimension need to be cast to homogeneous coord")
    elif len(point_coords) == 3:
        point_coords = np.append(point_coords,1.0)

    fox = 13
    foy = 13
##    K = np.array(([fox / (4.8 * 0.001),0,-640],[0,foy/ (4.8 * 0.001),-512],[0,0,-1]))
    K = np.array(([fox / (4.8 * 0.001),0,0],[0,foy/ (4.8 * 0.001),0],[0,0,1]))

    RRC = np.loadtxt("R_normalized.txt",delimiter=' ')
    transform = np.array([0,0,1000])
    transform= transform.reshape((3,1))
    RRC = np.append(RRC,transform,axis=1)
    
    P = np.dot(K, RRC)

    result = np.dot(P, point_coords.T)
    result = result / result[-1]
    result[1] = result[1] * -1
    
    result = result + np.array(([640,512,0]))
    negative = np.where(result < 0)
    positive = np.where(result > 0)

    
    result[negative] = np.ceil(result[negative])
    result[positive] = np.floor(result[positive])
##    result = (np.floor(result * 1.0) ).astype(int)
##    print(result)
    return((result[0:2]).astype(int))

if __name__ == '__main__':
    print(projection(([0,0,0])))
    a = [1,2,3]
    b = [4,5,6]

    for i, j in zip(a,b):
        print("i ",i,"j",j)
