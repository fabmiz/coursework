#!/usr/bin/python
#Fabrice Mizero
#CS 6501 - Machine Learning & Data Mining
from __future__ import division
import numpy as np
import glob
import sys
from sklearn.naive_bayes import MultinomialNB

textDataSetsDirectoryFullPath=sys.argv[1]
testFileDirectoryFullPath=sys.argv[2]

class naiveBayesMulFeature:
	def __init__(self):
		pass

	@staticmethod
	def transfer(fileDj,vocabulary):
		feature_row={}
		row=[]
		love_words=["love","loved","loves"]
		for word_ in vocabulary:
			feature_row[word_]=0
		with open(fileDj,'r') as f:
			for line in f:
				line_=line.split(' ')
				for word in line_:
					if word in love_words: feature_row["love"]+=1
					elif word in vocabulary: feature_row[word]+=1
		row=[feature_row[word] for word in vocabulary]
		return row

	@staticmethod
	def loadData(path):
		Xtrain=[]
		Xtest=[]
		ytrain=[]
		ytest=[]
		X={}
		y={}
		for set_ in ["training","test"]:
			for target in ["pos","neg"]:
				X[(set_, target)]=[]
				y[(set_, target)]=[]
		for set_ in ["training","test"]:
			for target in ["pos","neg"]:
				for file_ in glob.glob(path+set_+"_set/"+target+"/*.txt"):
					X[(set_, target)].append(naiveBayesMulFeature.transfer(file_,vocabulary))
					y[(set_,target)].append(target)
		Xtrain=np.vstack((X[("training","pos")],X[("training","neg")]))
		ytrain=np.hstack((y[("training","pos")],y[("training","neg")]))
		Xtest=np.vstack((X[("test","pos")],X[("test","neg")]))
		ytest=np.hstack((y[("test","pos")],y[("test","neg")]))

		return Xtrain,Xtest,ytrain,ytest

	@staticmethod
	def train(Xtrain,ytrain):
		XYtrain=np.column_stack((Xtrain,ytrain))
		Xtrain_pos=[]
		Xtrain_neg=[]
		for row in XYtrain:
			if row[-1] == "pos":
				Xtrain_pos.append(row[:-1])
			else:
				Xtrain_neg.append(row[:-1])
		column_sums_pos=np.sum(np.array(Xtrain_pos,dtype=int),axis=0)
		column_sums_neg=np.sum(np.array(Xtrain_neg,dtype=int),axis=0)
		n_pos=np.sum(np.array(column_sums_pos))
		n_neg=np.sum(np.array(column_sums_neg))
		thetaPos=[(n_k+1)/(n_pos+14) for n_k in column_sums_pos]
		thetaNeg=[(n_k+1)/(n_neg+14) for n_k in column_sums_neg]

		return thetaPos,thetaNeg

	@staticmethod
	def test(Xtest,ytest):
		yPredict=[]
		log_pos=[np.log(p) for p in thetaPos]
		log_neg=[np.log(p) for p in thetaNeg]
		for row in Xtest:
			p_pos=np.sum(np.array(log_pos)*np.array(row))
			p_neg=np.sum(np.array(log_neg)*np.array(row))
			if np.max([p_pos,p_neg]) == p_pos: result="pos"
			else: result="neg"
			yPredict.append(result)
		hits=0
		for i in xrange(len(ytest)):
			if yPredict[i] == ytest[i]:
				hits+=1
		return yPredict,(hits/len(ytest))*100

	@staticmethod
	def testDirectOne(full_path_test_file):
		log_pos=[np.log(p) for p in thetaPos]
		log_neg=[np.log(p) for p in thetaNeg]
		row=naiveBayesMulFeature.transfer(full_path_test_file,vocabulary)
		p_pos=np.sum(np.array(log_pos)*np.array(row))
		p_neg=np.sum(np.array(log_neg)*np.array(row))
		if np.max([p_pos,p_neg]) == p_pos: result="pos"
		else: result="neg"
		return result

	@staticmethod
	def testDirect(test_full_Directory):
		f_pos =[naiveBayesMulFeature.testDirectOne(file_) for file_ in glob.glob(test_full_Directory+"pos/"+"*.txt")]
		f_neg =[naiveBayesMulFeature.testDirectOne(file_) for file_ in glob.glob(test_full_Directory+"neg/"+"*.txt")]
		yPredict=np.hstack((f_pos,f_neg))
		hits=0
		print len(ytest)
		print len(yPredict)
		for i in xrange(len(ytest)):
			if yPredict[i] == ytest[i]:
				hits+=1
		return yPredict,(hits/len(ytest))*100

class naiveBayesBernFeature:
	def __init__(self):
		pass

	@staticmethod
	def train(Xtrain,ytrain):
		for row in Xtrain:
			for i in xrange(14):
				if row[i] != 0: row[i]=1
		XYtrain=np.column_stack((Xtrain,ytrain))
		Xtrain_pos=[row[:-1] for row in XYtrain if row[-1] == "pos"]
		Xtrain_neg=[row[:-1] for row in XYtrain if row[-1] == "neg"]
		column_sums_pos=np.sum(np.array(Xtrain_pos,dtype=int),axis=0)
		column_sums_neg=np.sum(np.array(Xtrain_neg,dtype=int),axis=0)
		n_pos=np.array(Xtrain_pos).shape[0]
		n_neg=np.array(Xtrain_neg).shape[0]
		thetaPos=[(n_k+1)/(n_pos+2) for n_k in column_sums_pos]
		thetaNeg=[(n_k+1)/(n_neg+2) for n_k in column_sums_neg]

		return thetaPos,thetaNeg

	@staticmethod
	def test(Xtest,ytest):
		yPredict=[]
		for row in Xtest:
			log_pos=0
			log_neg=0
			for i in xrange(14):
				if row[i] != 0:
					log_pos += np.log(thetaPos[i])
					log_neg += np.log(thetaNeg[i])
				else:
					log_pos += np.log(1-thetaPos[i])
					log_neg += np.log(1-thetaNeg[i])

			if np.max([log_pos,log_neg]) == log_pos: result="pos"
			else: result="neg"
			yPredict.append(result)

		hits=0
		for i in xrange(len(ytest)):
			if yPredict[i] == ytest[i]:
				hits+=1
		return yPredict,(hits/len(ytest))*100

vocabulary=["love", "wonderful", "best", "great", "superb", "still", "beautiful", "bad", "worst", "stupid", "waste", "boring","?","!"]

Xtrain,Xtest,ytrain,ytest= naiveBayesMulFeature.loadData(textDataSetsDirectoryFullPath)
thetaPos,thetaNeg= naiveBayesMulFeature.train(Xtrain,ytrain)

print "Q5."
print "thetaPos"
print thetaPos
print "thetaNeg"
print thetaNeg


yPredict,accuracy = naiveBayesMulFeature.test(Xtest,ytest)
print "Q6. Accuracy - naiveBayesMulFeature.test()"
print accuracy

yPredict,accuracy = naiveBayesMulFeature.testDirect(testFileDirectoryFullPath)
print "Q9. Accuracy -  naiveBayesMulFeature.testDirect()"
print accuracy


thetaPosTrue,thetaNegTrue=naiveBayesBernFeature.train(Xtrain,ytrain)
print "Q10. thetaPos,thetaNeg - naiveBayesBernFeature.train()"
print "thetaPosTrue"
print thetaPosTrue
print "thetaNegTrue"
print thetaNegTrue

yPredict,accuracy = naiveBayesBernFeature.test(Xtest,ytest)
print "Q11. Accuracy -  naiveBayesBernFeature.test()"
print accuracy
